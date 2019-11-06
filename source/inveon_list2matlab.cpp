/**************************************************************************
* Loads the Siemens Inveon 48-bit list-mode data and stores the detector
* indices of prompts and delays at specific time steps.
*
* Copyright (C) 2019 Ville-Veikko Wettenhovi
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
***************************************************************************/

#include <cstdint>
#include <cmath>
#include <iostream>
#include <fstream>
#include "mex.h"
#include <algorithm>

FILE *streami;

void histogram(uint16_t * LL1, uint16_t * LL2, uint32_t * tpoints, char **argv, const double vali, const double alku, const double loppu, const size_t outsize2,
	const uint32_t detectors, const size_t pituus, const bool randoms_correction, uint16_t *DD1, uint16_t *DD2, uint8_t *TOF)
{

	static long long int i, i1 = -1;//__int64

	static char *in_file;

	static int16_t qb;
	static int prompt;
	static uint64_t ew1;
	static int tag;

	double ms = 0;		// seconds

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
	errno_t err;
	err = fopen_s(&streami, argv[0], "rb");
	if (err != 0) {
		mexPrintf("No file opened %s\n", in_file);
		exit(1);
	}
#else
	streami = fopen(argv[0], "rb");
	if (streami == NULL) {
		mexPrintf("No file opened %s\n", in_file);
		exit(1);
	}
#endif

	uint32_t L1;    //detector 1
	uint32_t L2;    //detector 2
	int mscount = 0;
	int ll = -1;
	double aika = alku + vali;
	bool begin = false;
	if (outsize2 > 1)
		begin = true;

	mexPrintf("file opened \n");
	while ((i = fread(&ew1, sizeof(qb) * 3, 1, streami)) != 0 && ms <= loppu) {

		i1++;

		tag = ((ew1 >> (43)) & 1);

		prompt = 0;

		if (!tag) {
			if (ms >= alku) {
				prompt = (ew1 >> (42)) & 1;
				ll++;
				if (prompt) {
					if (begin) {
						tpoints[mscount] = ll;
						mscount++;
						begin = false;
					}
					L1 = (ew1 >> 19) & 0x1ffff;
					L2 = ew1 & 0x1ffff;
					if (L1 >= detectors || L2 >= detectors)
						continue;
					if (outsize2 == 1) {
						LL1[L1*detectors + L2] = LL1[L1*detectors + L2] + static_cast<uint16_t>(1);
					}
					else {
						LL1[ll] = static_cast<uint16_t>(L1 + 1);
						LL2[ll] = static_cast<uint16_t>(L2 + 1);
					}
				}
				else if (randoms_correction && prompt == 0) {
					L1 = (ew1 >> 19) & 0x1ffff;
					L2 = ew1 & 0x1ffff;
					if (L1 >= detectors || L2 >= detectors)
						continue;
					if (outsize2 == 1) {
						DD1[L1*detectors + L2] = DD1[L1*detectors + L2] + static_cast<uint16_t>(1);
					}
					else {
						DD1[ll] = static_cast<uint16_t>(L1 + 1);
						DD2[ll] = static_cast<uint16_t>(L2 + 1);
					}
				}
			}	
		}
		if (tag) {
			if (((ew1 >> (36)) & 0xff) == 160) { // Elapsed Time Tag Packet
				ms += 200e-6; // 200 microsecond increments

				if (ms >= aika) {
					tpoints[mscount] = ll;
					mscount++;
					aika += (vali);
				}
			}
		}
	}
	mexPrintf("End time %f\n", ms);
	mexEvalString("pause(.0001);");
	tpoints[mscount] = ll;
	fclose(streami);
	return;
}


void mexFunction(int nlhs, mxArray *plhs[],
	int nrhs, const mxArray*prhs[])

{


	/* Check for proper number of arguments */

	if (nrhs != 7) {
		mexErrMsgIdAndTxt("MATLAB:list2matlab_aivi:invalidNumInputs",
			"Seven input arguments required.");
	}
	else if (nlhs > 6) {
		mexErrMsgIdAndTxt("MATLAB:list2matlab_aivi:maxlhs",
			"Too many output arguments.");
	}

	double vali = (double)mxGetScalar(prhs[1]);
	double alku = (double)mxGetScalar(prhs[2]);
	double loppu = (double)mxGetScalar(prhs[3]);
	size_t pituus = (size_t)mxGetScalar(prhs[4]);
	uint32_t detectors = (uint32_t)mxGetScalar(prhs[5]);
	bool randoms_correction = (bool)mxGetScalar(prhs[6]);
	//double energy_low1 = (double)mxGetScalar(prhs[7]);
	//double energy_low2 = (double)mxGetScalar(prhs[8]);
	//double energy_high1 = (double)mxGetScalar(prhs[9]);
	//double energy_high2 = (double)mxGetScalar(prhs[10]);
	//double energy_normal1 = (double)mxGetScalar(prhs[11]);
	//double energy_normal2 = (double)mxGetScalar(prhs[12]);
	size_t outsize2 = (loppu - alku) / vali;

	if (outsize2 == 1) {
		plhs[0] = mxCreateNumericMatrix(detectors, detectors, mxUINT16_CLASS, mxREAL);
		plhs[1] = mxCreateNumericMatrix(1, 1, mxUINT16_CLASS, mxREAL);
		if (randoms_correction) {
			plhs[3] = mxCreateNumericMatrix(detectors, detectors, mxUINT16_CLASS, mxREAL);
			plhs[4] = mxCreateNumericMatrix(1, 1, mxUINT16_CLASS, mxREAL);
		}
		else {
			plhs[3] = mxCreateNumericMatrix(1, 1, mxUINT16_CLASS, mxREAL);
			plhs[4] = mxCreateNumericMatrix(1, 1, mxUINT16_CLASS, mxREAL);
		}
	}
	else {
		plhs[0] = mxCreateNumericMatrix(pituus, 1, mxUINT16_CLASS, mxREAL);
		plhs[1] = mxCreateNumericMatrix(pituus, 1, mxUINT16_CLASS, mxREAL);
		if (randoms_correction) {
			plhs[3] = mxCreateNumericMatrix(pituus, 1, mxUINT16_CLASS, mxREAL);
			plhs[4] = mxCreateNumericMatrix(pituus, 1, mxUINT16_CLASS, mxREAL);
		}
		else {
			plhs[3] = mxCreateNumericMatrix(1, 1, mxUINT16_CLASS, mxREAL);
			plhs[4] = mxCreateNumericMatrix(1, 1, mxUINT16_CLASS, mxREAL);
		}
	}
	plhs[2] = mxCreateNumericMatrix(outsize2 + 2, 1, mxUINT32_CLASS, mxREAL);
	plhs[5] = mxCreateNumericMatrix(pituus, 1, mxUINT8_CLASS, mxREAL);


	/* Assign pointers to the various parameters */
	uint16_t * LL1 = (uint16_t*)mxGetData(plhs[0]);
	uint16_t * LL2 = (uint16_t*)mxGetData(plhs[1]);
	uint32_t * tpoints = (uint32_t*)mxGetData(plhs[2]);
	uint16_t * DD1 = (uint16_t*)mxGetData(plhs[3]);
	uint16_t * DD2 = (uint16_t*)mxGetData(plhs[4]);
	uint8_t * TOF = (uint8_t*)mxGetData(plhs[5]);

	// Count inputs and check for char type
	int argc = 0;
	char **argv;
	mwIndex i;
	int k, ncell;
	int j = 0;
	for (k = 0; k<1; k++)
	{
		if (mxIsCell(prhs[k]))
		{
			argc += ncell = mxGetNumberOfElements(prhs[k]);
			for (i = 0; i<ncell; i++)
				if (!mxIsChar(mxGetCell(prhs[k], i)))
					mexErrMsgTxt("Input cell element is not char");
		}
		else
		{
			argc++;
			if (!mxIsChar(prhs[k]))
				mexErrMsgTxt("Input argument is not char");
		}
	}

	/* Pointer to character array */
	argv = (char **)mxCalloc(argc, sizeof(char *));
	for (k = 0; k<1; k++)
	{
		if (mxIsCell(prhs[k]))
		{
			ncell = mxGetNumberOfElements(prhs[k]);
			for (i = 0; i<ncell; i++)
				argv[j++] = mxArrayToString(mxGetCell(prhs[k], i)
				);
		}
		else
		{
			argv[j++] = mxArrayToString(prhs[k]);
		}
	}

	histogram(LL1, LL2, tpoints, argv, vali, alku, loppu, outsize2, detectors, pituus, randoms_correction, DD1, DD2, TOF);
	return;

}