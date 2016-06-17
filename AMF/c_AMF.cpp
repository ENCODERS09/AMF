/********************************************************
 * c_AMF.cpp
 * C++ implements on AMF
 * Author: Jamie Zhu <jimzhu@GitHub>
 * Created: 2014/5/6
 * Last updated: 2016/4/30
********************************************************/

#include <iostream>
#include <cstring>
#include <cmath>
#include <vector>
#include <utility>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include "c_AMF.h"
using namespace std;

typedef pair<pair<int, int>, double> SAMPLE;
const double EPS = 1e-8;


inline double square(double x) {return x * x;}

/********************************************************
 * Udata, Sdata, predData are the output values
********************************************************/
void AMF(double *removedData, int numUser, int numService, int dim, 
    double lmda, int maxIter, double convergeThreshold, double eta, 
    double beta, bool debugMode, double *Udata, double *Sdata, double *p,
    double *q, double *predData)
{   
    // --- transfer the 1D pointer to 2D array pointer
    double **removedMatrix = vector2Matrix(removedData, numUser, numService);
    double **U = vector2Matrix(Udata, numUser, dim);
    double **S = vector2Matrix(Sdata, numService, dim);
    double **predMatrix = vector2Matrix(predData, numUser, numService);

    // --- transform removedMatrix into tuple samples
    vector<SAMPLE> samples;
    int numSample = 0; 
    for (int i = 0; i < numUser; i++) {
        for (int j = 0; j < numService; j++) {
            if (fabs(removedMatrix[i][j]) > EPS) {
                samples.push_back(make_pair(make_pair(i, j), removedMatrix[i][j]));
                numSample++;
            }
        }
    }

    // --- iterate by standard gradient descent algorithm
    SAMPLE spInstance;
    int iter = 0, minIter = 30;
    int i, j;
    double rValue, lossValue = 1e10, gradU, gradS, gradP, gradQ;
    long double eij, wi, wj;
    vector<long double> eu(numUser, 1), es(numService, 1);
    srand((unsigned)time(NULL));

    while(lossValue > convergeThreshold || iter < minIter) { 
        // max iteration
        if (iter >= maxIter) {
            break;                       
        }

        // random shuffle of sample
        random_shuffle(samples.begin(), samples.end());

        // one iteration
        for (int s = 0; s < numSample; s++) {
            spInstance = samples[s];
            i = spInstance.first.first;
            j = spInstance.first.second;
            rValue = spInstance.second;

            // confidence updates
            long double qos = dotProduct(U[i], S[j], dim) + p[i] + q[j];
            double pValue = sigmoid(qos);
            eij = fabs(pValue - rValue) / rValue;
            wi = eu[i] / (eu[i] + es[j]);
            wj = es[j] / (eu[i] + es[j]);
            eu[i] = beta * wi * eij + (1 - beta * wi) * eu[i];
            es[j] = beta * wj * eij + (1 - beta * wj) * es[j];

            // gradient descent updates
            long double grad_sigmoid_qos = grad_sigmoid(qos);
            for (int k = 0; k < dim; k++) {
                gradU = wi * (pValue - rValue) * grad_sigmoid_qos * S[j][k]
                    + lmda * U[i][k];
                gradS = wj * (pValue - rValue) * grad_sigmoid_qos * U[i][k]
                    + lmda * S[j][k];
                U[i][k] -= eta * gradU;
                S[j][k] -= eta * gradS;
            }
            gradP = wi * (pValue - rValue) * grad_sigmoid_qos + lmda * p[i];
            gradQ = wi * (pValue - rValue) * grad_sigmoid_qos + lmda * q[j];
            p[i] -= eta * gradP;
            q[j] -= eta * gradQ;
        }

        // update predMatrix and loss value
        getPredMatrix(false, removedMatrix, U, S, p, q, numUser, numService, dim, predMatrix);
        lossValue = loss(U, S, p, q, removedMatrix, predMatrix, lmda, numUser, numService, dim);
        lossValue = lossValue / numSample;

        // log the debug info to check convergence   
        if (debugMode) {
            cout.setf(ios::fixed);            
            cout << currentDateTime() << ": ";
            cout << "iter = " << iter << ", lossValue = " << lossValue << endl;
        }

        iter++;
    }

    // update predMatrix
    getPredMatrix(true, removedMatrix, U, S, p, q, numUser, numService, dim, predMatrix);

    delete ((char*) U);
    delete ((char*) S);
    delete ((char*) removedMatrix);
    delete ((char*) predMatrix);
}


double loss(double **U, double **S, double *p, double *q, double **removedMatrix, double **predMatrix, double lmda, 
    int numUser, int numService, int dim)
{
    int i, j, k;
    double loss = 0;

    // cost
    for (i = 0; i < numUser; i++) {
        for (j = 0; j < numService; j++) {
            if (fabs(removedMatrix[i][j]) > EPS) {
                loss += 0.5 * square((removedMatrix[i][j] - predMatrix[i][j]) / removedMatrix[i][j]);  
            }
        }
    }

    // L2 regularization
    for (k = 0; k < dim; k++) {
        for (i = 0; i < numUser; i++) {
            loss += 0.5 * lmda * square(U[i][k]);
        }
        for (j = 0; j < numService; j++) {
            loss += 0.5 * lmda * square(S[j][k]);
        }
    }
    for (i = 0; i < numUser; i++) {
        loss += 0.5 * lmda * square(p[i]);
    }
    for (j = 0; j < numService; j++) {
        loss += 0.5 * lmda * square(q[j]);
    }

    return loss;
}


void getPredMatrix(bool flag, double **removedMatrix, double **U, double **S, double *p, double *q, 
    int numUser, int numService, int dim, double **predMatrix)
{
    int i, j;
    for (i = 0; i < numUser; i++) {
        for (j = 0; j < numService; j++) {
            if (flag == true || fabs(removedMatrix[i][j]) > EPS) {
                predMatrix[i][j] = sigmoid(p[i] + q[j] + dotProduct(U[i], S[j], dim));
            } 
        }
    }
}


double sigmoid(long double x)
{
    return 1 / (1 + exp(-x));
}


long double grad_sigmoid(long double x)
{
    return 1 / (2 + exp(-x) + exp(x));
}


long double dotProduct(double *vec1, double *vec2, int len)  
{
    long double product = 0;
    for (int i = 0; i < len; i++) {
        product += vec1[i] * vec2[i];
    }
    return product;
}


double **vector2Matrix(double *vector, int row, int col)  
{
    double **matrix = new double *[row];
    if (!matrix) {
        cout << "Memory allocation failed in vector2Matrix." << endl;
        return NULL;
    }

    for (int i = 0; i < row; i++) {
        matrix[i] = vector + i * col;  
    }
    return matrix;
}


double **createMatrix(int row, int col) 
{
    double **matrix = new double *[row];
    matrix[0] = new double[row * col];
    memset(matrix[0], 0, row * col * sizeof(double)); // Initialization
    int i;
    for (i = 1; i < row; i++) {
        matrix[i] = matrix[i - 1] + col;
    }
    return matrix;
}


void delete2DMatrix(double **ptr)
{
    delete ptr[0];
    delete ptr;
}


void copyMatrix(double **M1, double **M2, int row, int col)
{
    for (int i = 0; i < row; i++) {
        for (int j = 0; j < col; j++) {
            M1[i][j] = M2[i][j];
        }
    }
}


const string currentDateTime() 
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);

    return buf;
}


