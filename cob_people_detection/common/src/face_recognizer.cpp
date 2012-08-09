/*!
*****************************************************************
* \file
*
* \note
* Copyright (c) 2012 \n
* Fraunhofer Institute for Manufacturing Engineering
* and Automation (IPA) \n\n
*
*****************************************************************
*
* \note
* Project name: Care-O-bot
* \note
* ROS stack name: cob_people_perception
* \note
* ROS package name: cob_people_detection
*
* \author
* Author: Richard Bormann
* \author
* Supervised by:
*
* \date Date of creation: 07.08.2012
*
* \brief
* functions for recognizing a face within a color image (patch)
* current approach: eigenfaces on color image
*
*****************************************************************
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* - Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer. \n
* - Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution. \n
* - Neither the name of the Fraunhofer Institute for Manufacturing
* Engineering and Automation (IPA) nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission. \n
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License LGPL as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License LGPL for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License LGPL along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*
****************************************************************/


#ifdef __LINUX__
	#include "cob_people_detection/face_recognizer.h"
	#include "cob_vision_utils/GlobalDefines.h"
#else
#include "cob_vision/cob_people_detection/common/include/cob_people_detection/PeopleDetector.h"
#include "cob_common/cob_vision_utils/common/include/cob_vision_utils/GlobalDefines.h"
#endif

// stream
#include <fstream>

// opencv
#include <opencv/cv.h>
#include <opencv/cvaux.h>


using namespace ipa_PeopleDetector;

FaceRecognizer::FaceRecognizer(void)
{
}

unsigned long FaceRecognizer::init()
{	

	return ipa_Utils::RET_OK;
}

FaceRecognizer::~FaceRecognizer(void)
{
}

unsigned long FaceRecognizer::AddFace(cv::Mat& img, cv::Rect& face, std::string id, std::vector<cv::Mat>& images, std::vector<std::string>& ids)
{
	//IplImage *resized_8U1 = cvCreateImage(cvSize(100, 100), 8, 1);
	cv::Mat resized_8U1(100, 100, CV_8UC1);
	ConvertAndResize(img, resized_8U1, face);
	
	// Save image
	images.push_back(resized_8U1);
	ids.push_back(id);

	return ipa_Utils::RET_OK;
}

unsigned long FaceRecognizer::convertAndResize(cv::Mat& img, cv::Mat& resized, cv::Rect& face, cv::Size new_size)
{
	cv::Mat temp;
	cv::cvtColor(img, temp, CV_BGR2GRAY);
	cv::Mat roi = temp(face);
	cv::resize(roi, resized, new_size);

	return ipa_Utils::RET_OK;
}

cv::Mat FaceRecognizer::preprocessImage(cv::Mat& input_image)
{
	// todo:
	return input_image;

	// do a modified census transform
	cv::Mat output(input_image.cols, input_image.rows, input_image.type());
	//cv::Mat smoothedImage = input_image.clone();
	//cv::GaussianBlur(smoothedImage, smoothedImage, cv::Size(3,3), 0, 0, cv::BORDER_REPLICATE);

	for (int v=0; v<input_image.rows; v++)
	{
		uchar* srcPtr = input_image.ptr(v);
		//uchar* smoothPtr = smoothedImage.ptr(v);
		uchar* outPtr = output.ptr(v);
		for (int u=0; u<input_image.cols; u++)
		{
			int ctOutcome = 0;
			int offset = -1;
			for (int dv=-1; dv<=1; dv++)
			{
				for (int du=-1; du<=1; du++)
				{
					if (dv==0 && du==0) continue;
					offset++;
					if (v+dv<0 || v+dv>=input_image.rows || u+du<0 || u+du>=input_image.cols) continue;
					//if (*smoothPtr < *(srcPtr+dv*input_image.step+du)) ctOutcome += 1<<offset;
					if (*srcPtr < *(srcPtr+dv*input_image.step+du)) ctOutcome += 1<<offset;
				}
			}
			*outPtr = ctOutcome;

			srcPtr++;
			outPtr++;
		}
	}

//	cv::imshow("census transform", output);
//	cv::waitKey();

	return output;
}

unsigned long FaceRecognizer::PCA(int* nEigens, std::vector<cv::Mat>& eigenVectors, cv::Mat& eigenValMat, cv::Mat& avgImage, std::vector<cv::Mat>& faceImages, cv::Mat& projectedTrainFaceMat)
{
	CvTermCriteria calcLimit;

	// Set the number of eigenvalues to use
	(*nEigens) = faceImages.size()-1;
	
	// Allocate memory
	cv::Size faceImgSize(faceImages[0].cols, faceImages[0].rows);
	eigenVectors.resize(*nEigens, cv::Mat(faceImgSize, CV_32FC1));
	m_eigen_val_mat.create(1, *nEigens, CV_32FC1);
	avgImage.create(faceImgSize, CV_32FC1);

	// Set the PCA termination criterion
	calcLimit = cvTermCriteria(CV_TERMCRIT_ITER, (*nEigens), 1);

	// Convert vector to array
	IplImage** faceImgArr = (IplImage**)cvAlloc((int)faceImages.size()*sizeof(IplImage*));
	for(int j=0; j<(int)faceImages.size(); j++)
	{
		// todo: preprocess
		cv::Mat preprocessedImage = preprocessImage(faceImages[j]);
		IplImage temp = (IplImage)preprocessedImage;
		faceImgArr[j] = cvCloneImage(&temp);
	}

	// Convert vector to array
	IplImage** eigenVectArr = (IplImage**)cvAlloc((int)eigenVectors.size()*sizeof(IplImage*));
	for(int j=0; j<(int)eigenVectors.size(); j++)
	{
		IplImage temp = (IplImage)eigenVectors[j];
		eigenVectArr[j] = cvCloneImage(&temp);
	}

	// Compute average image, eigenvalues, and eigenvectors
	IplImage avgImageIpl = (IplImage)avgImage;
	cvCalcEigenObjects((int)faceImages.size(), (void*)faceImgArr, (void*)eigenVectArr, CV_EIGOBJ_NO_CALLBACK, 0, 0, &calcLimit, &avgImageIpl, (float*)(m_eigen_val_mat.data));

	// todo:
	cv::normalize(m_eigen_val_mat,m_eigen_val_mat, 1, 0, /*CV_L1*/CV_L2);	//, 0);		0=bug?

	// Project the training images onto the PCA subspace
	projectedTrainFaceMat.create(faceImages.size(), *nEigens, CV_32FC1);
	for(int i=0; i<(int)faceImages.size(); i++)
	{
		IplImage temp = (IplImage)faceImages[i];
		cvEigenDecomposite(&temp, *nEigens, eigenVectArr, 0, 0, &avgImageIpl, (float*)projectedTrainFaceMat.data + i* *nEigens);	//attention: if image step of projectedTrainFaceMat is not *nEigens * sizeof(float) then reading functions which access with (x,y) coordinates might fail
	};

	// Copy back
	int eigenVectorsCount = (int)eigenVectors.size();
	eigenVectors.clear();
	for (int i=0; i<(int)eigenVectorsCount; i++) eigenVectors.push_back(cv::Mat(eigenVectArr[i], true));

	// Clean
	for (int i=0; i<(int)faceImages.size(); i++) cvReleaseImage(&(faceImgArr[i]));
	for (int i=0; i<(int)eigenVectors.size(); i++) cvReleaseImage(&(eigenVectArr[i]));
	cvFree(&faceImgArr);
	cvFree(&eigenVectArr);

	return ipa_Utils::RET_OK;
}

unsigned long FaceRecognizer::RecognizeFace(cv::Mat& colorImage, std::vector<cv::Rect>& colorFaceCoordinates, std::vector<int>& index, cv::SVM* personClassifier)
{
	int number_eigenvectors = m_eigen_vectors.size();
	float* eigenVectorWeights = 0;
	eigenVectorWeights = (float *)cvAlloc(number_eigenvectors*sizeof(float));

	// Convert vector to array
	IplImage** eigenVectArr = (IplImage**)cvAlloc(number_eigenvectors*sizeof(IplImage*));
	for(int j=0; j<number_eigenvectors; j++)
	{
		IplImage temp = (IplImage)m_eigen_vectors[j];
		eigenVectArr[j] = cvCloneImage(&temp);
	}
	
	cv::Mat resized_8U1;
	cv::Size resized_size(100, 100);
	for(int i=0; i<(int)colorFaceCoordinates.size(); i++)
	{
		cv::Rect face = colorFaceCoordinates[i];
		convertAndResize(colorImage, resized_8U1, face, resized_size);
		// todo: preprocess
		cv::Mat preprocessedImage = preprocessImage(resized_8U1);

		IplImage avgImageIpl = (IplImage)m_avg_image;
		
		// Project the test image onto the PCA subspace
		IplImage resized_8U1Ipl = (IplImage)resized_8U1;
		cvEigenDecomposite(&resized_8U1Ipl, m_n_eigens, eigenVectArr, 0, 0, &avgImageIpl, eigenVectorWeights);

		// Calculate FaceSpace Distance
		cv::Mat srcReconstruction = cv::Mat::zeros(m_eigen_vectors[0].size(), m_eigen_vectors[0].type());
		for(int i=0; i<number_eigenvectors; i++) srcReconstruction += eigenVectorWeights[i]*m_eigen_vectors[i];
		cv::Mat temp;

		// todo:
//		cv::Mat reconstrTemp = srcReconstruction + m_avg_image;
//		cv::Mat reconstr(m_eigen_vectors[0].size(), CV_8UC1);
//		reconstrTemp.convertTo(reconstr, CV_8UC1, 1);
//		cv::imshow("reconstruction", reconstr);
//		cv::waitKey();

		resized_8U1.convertTo(temp, CV_32FC1, 1.0/255.0);
		double distance = cv::norm((temp-m_avg_image), srcReconstruction, cv::NORM_L2);

		//######################################## Only for debugging and development ########################################
		//std::cout.precision( 10 );
		std::cout << "FS_Distance: " << distance << std::endl;
		//######################################## /Only for debugging and development ########################################

		// -2=distance to face space is too high
		// -1=distance to face classes is too high
		if(distance > m_threshold_facespace)
		{
			// No face
			index.push_back(-2);
			//index.push_back(-2); why twice? apparently makes no sense.
		}
		else
		{
			int nearest;
			ClassifyFace(eigenVectorWeights, &nearest, nEigens, faceClassAvgProjections, threshold, eigenValMat, personClassifier);
			if(nearest < 0) index.push_back(-1);	// Face Unknown
			else index.push_back(nearest);	// Face known, it's number nearest
		}
	}

	// Clear
	for (int i=0; i<number_eigenvectors; i++) cvReleaseImage(&(eigenVectArr[i]));
	cvFree(&eigenVectorWeights);
	cvFree(&eigenVectArr);
	return ipa_Utils::RET_OK;
}

unsigned long FaceRecognizer::ClassifyFace(float *eigenVectorWeights, int *nearest, int *nEigens, cv::Mat& faceClassAvgProjections, int *threshold, cv::Mat& eigenValMat, cv::SVM* personClassifier)
{
	double leastDistSq = DBL_MAX;
	//todo:
	int metric = 2; 	// 0 = Euklid, 1 = Mahalanobis, 2 = Mahalanobis Cosine


	for(int i=0; i<m_face_class_avg_projections.rows; i++)
	{
		double distance=0;
		double cos=0;
		double length_sample=0;
		double length_projection=0;
		for(int e=0; e<*nEigens; e++)
		{
			if (metric < 2)
			{
				float d = eigenVectorWeights[e] - ((float*)(m_face_class_avg_projections.data))[i * *nEigens + e];
				if (metric==0) distance += d*d;							//Euklid
				else distance += d*d /* / */ / ((float*)(m_eigen_val_mat.data))[e];	//Mahalanobis
			}
			else
			{
				cos += eigenVectorWeights[e] * ((float*)(m_face_class_avg_projections.data))[i * *nEigens + e] / ((float*)(m_eigen_val_mat.data))[e];
				length_projection += ((float*)(m_face_class_avg_projections.data))[i * *nEigens + e] * ((float*)(m_face_class_avg_projections.data))[i * *nEigens + e] / ((float*)(m_eigen_val_mat.data))[e];
				length_sample += eigenVectorWeights[e]*eigenVectorWeights[e] / ((float*)(m_eigen_val_mat.data))[e];
			}
		}
		if (metric < 2)
			distance = sqrt(distance);
		else
		{
			length_sample = sqrt(length_sample);
			length_projection = sqrt(length_projection);
			cos /= (length_projection * length_sample);
			distance = -cos;
		}

		//######################################## Only for debugging and development ########################################
		//std::cout.precision( 10 );
		std::cout << "Distance_FC: " << distance << std::endl;
		//######################################## /Only for debugging and development ########################################

		if(distance < leastDistSq)
		{
			leastDistSq = distance;
			if(leastDistSq > m_threshold_unknown) *nearest = -1;
			else *nearest = i;
		}
	}

	// todo:
//	if (personClassifier != 0 && *nearest != -1)
//	{
//		cv::Mat temp(1, *nEigens, CV_32FC1, eigenVectorWeights);
//		std::cout << "class. output: " << (int)personClassifier->predict(temp) << "\n";
//		*nearest = (int)personClassifier->predict(temp);
//	}

	return ipa_Utils::RET_OK;
}

unsigned long FaceRecognizer::CalculateFaceClasses(cv::Mat& projectedTrainFaceMat, std::vector<std::string>& id, int *nEigens, cv::Mat& faceClassAvgProjections,
		std::vector<std::string>& idUnique, cv::SVM* personClassifier)
{
	std::cout << "PeopleDetector::CalculateFaceClasses ... ";

	// Look for face classes
	idUnique.clear();
	for(int i=0; i<(int)id.size(); i++)
	{
		std::string face_class = id[i];
		bool class_exists = false;
		
		for(int j=0; j<(int)idUnique.size(); j++)
		{
			if(!idUnique[j].compare(face_class))
			{
				class_exists = true;
			}
		}

		if(!class_exists)
		{
			idUnique.push_back(face_class);
		}
	}

	//id.clear();
	//cv::Mat faces_tmp = projectedTrainFaceMat.clone();
	cv::Mat temp = cv::Mat::zeros((int)idUnique.size(), *nEigens, projectedTrainFaceMat.type());
	temp.convertTo(m_face_class_avg_projections, projectedTrainFaceMat.type());
	//for (int i=0; i<((int)idUnique.size() * *nEigens); i++)
//	for (int i=0; i<((int)id.size() * *nEigens); i++)
//	{
//		((float*)(projectedTrainFaceMat.data))[i] = 0;
//	}

	// Look for FaceClasses
//	for(int i=0; i<(int)idUnique.size(); i++)
//	{
//		std::string face_class = idUnique[i];
//		bool class_exists = false;
//
//		for(int j=0; j<(int)id.size(); j++)
//		{
//			if(!id[j].compare(face_class))
//			{
//				class_exists = true;
//			}
//		}
//
//		if(!class_exists)
//		{
//			id.push_back(face_class);
//		}
//	}

//	cv::Size newSize(id.size(), *nEigens);
//	projectedTrainFaceMat.create(newSize, faces_tmp.type());

	// Calculate FaceClasses
	for(int i=0; i<(int)idUnique.size(); i++)
	{
		std::string face_class = idUnique[i];
		
		for(int e=0; e<*nEigens; e++)
		{
			int count=0;
			for(int j=0;j<(int)id.size(); j++)
			{
				if(!(id[j].compare(face_class)))
				{
					((float*)(m_face_class_avg_projections.data))[i * *nEigens + e] += ((float*)(projectedTrainFaceMat.data))[j * *nEigens + e];
					count++;
				}
			}
			((float*)(m_face_class_avg_projections.data))[i * *nEigens + e] /= (float)count;
		}
	}


	// todo: machine learning technique for person identification
	if (personClassifier != 0)
	{
		//std::cout << "\n";
		// prepare ground truth
		cv::Mat data(id.size(), *nEigens, CV_32FC1);
		cv::Mat labels(id.size(), 1, CV_32SC1);
		std::ofstream fout("svm.dat", std::ios::out);
		for(int sample=0; sample<(int)id.size(); sample++)
		{
			// copy data
			for (int e=0; e<*nEigens; e++)
			{
				data.at<float>(sample, e) = ((float*)projectedTrainFaceMat.data)[sample * *nEigens + e];
				fout << data.at<float>(sample, e) << "\t";
			}
			// find corresponding label
			for(int i=0; i<(int)idUnique.size(); i++)	// for each person
				if(!(id[sample].compare(idUnique[i])))		// compare the labels
					labels.at<int>(sample) = i;					// and assign the corresponding label's index from the idUnique list
			fout << labels.at<int>(sample) << "\n";
		}
		fout.close();

		// train the classifier
		cv::SVMParams svmParams(CvSVM::NU_SVC, CvSVM::RBF, 0.0, 0.001953125, 0.0, 0.0, 0.8, 0.0, 0, cv::TermCriteria(CV_TERMCRIT_ITER+CV_TERMCRIT_EPS, 100, FLT_EPSILON));
		//personClassifier->train_auto(data, labels, cv::Mat(), cv::Mat(), svmParams, 10, cv::SVM::get_default_grid(CvSVM::C), CvParamGrid(0.001953125, 2.01, 2.0), cv::SVM::get_default_grid(CvSVM::P), CvParamGrid(0.0125, 1.0, 2.0));
		personClassifier->train(data, labels, cv::Mat(), cv::Mat(), svmParams);
		cv::SVMParams svmParamsOptimal = personClassifier->get_params();
		std::cout << "Optimal SVM params: gamma=" << svmParamsOptimal.gamma << "  nu=" << svmParamsOptimal.nu << "\n";
	}

	std::cout << "done\n";

	return ipa_Utils::RET_OK;
}

