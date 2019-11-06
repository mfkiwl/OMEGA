/**************************************************************************
* A matrix free improved Siddon's for OSEM or MLEM. 
* This function calculates Summ = sum(A,1) (sum of every row) and 
* rhs = A*(y./(A'*x)), where A is the system matrix, y the measurements 
* and x the estimate/image.
*
* Used by implementation 3.
*
* INPUTS:
* MethodList = The type of reconstruction algorithms used (e.g. 2 means
* COSEM)
* d_raw = if 1 then raw list-mode data is used otherwise sinogram
* data
* d_h = power factor for ACOSEM,
* d_Nx/y/z = image size in x/y/z- dimension,
* d_dz/x/y = distance between adjecent voxels in z/x/y-dimension,
* d_bz/x/y = distance from the pixel space to origin (z/x/y-dimension),
* d_bzb = part in parenthesis of equation (9) in [1] precalculated when
* k = Nz,
* d_maxxx/yy = maximum distance of the pixel space from origin in
* x/y-dimension,
* d_zmax = maximum value of d_zdet,
* d_NSlices = the number of image slices,
* d_x/y/z_det = detector x/y/z-coordinates,
* d_size_x = the number of detector elements,
* d_TotSinos = Total number of sinograms,
* d_attenuation_correction = if attenuation is included this is 1 otherwise
* 0,
* d_normalization = if normalization is included this is 1 otherwise 0,
* d_randoms = if randoms/scatter correction is included this is 1
* otherwise 0,
* d_atten = attenuation data (images),
* d_norm = normalization coefficients,
* d_epps = a small constant to prevent division by zero,
* d_N = d_Nx * d_Ny * d_Nz,
* d_pseudos = location of pseudo rings,
* pRows = number of pseudo rings,
* d_Nxy = d_Nx * d_Ny,
* d_det_per_ring = number of detectors per ring,
* n_rekos = number of reconstruction algorithms used,
* dc_z = Distance between the rays (z-direction),
* n_rays = number of rays used,
* d_Summ = buffer for Summ,
* d_lor = number of pixels that each LOR traverses,
* d_xy/zindex = for sinogram format they determine the detector
* indices corresponding to each sinogram bin (unused with raw data),
* d_L = detector numbers for raw data (unused for sinogram format),
* d_epsilon_mramla = epsilon value for MRAMLA/MBSREM,
* d_Sino = Sinogram/raw data,
* d_sc_ra = Randoms and/or scatter data,
* d_OSEM = buffer for all estimates,
* d_rhs_OSEM = buffer for all RHS elements,
* no_norm = If 1, normalization constant is not computed,
* m_size = Total number of LORs for this subset,
* ax = Local buffer for forward projection data
*
* OUTPUTS:
* d_rhs_OSEM = rhs values for all algorithms/priors X,
* d_OSEM = estimates of all algorithm
* d_Summ = Normalization constant
*
* [1] Jacobs, F., Sundermann, E., De Sutter, B., Christiaens, M. Lemahieu,
* I. (1998). A Fast Algorithm to Calculate the Exact Radiological Path
* through a Pixel or Voxel Space. Journal of computing and information
* technology, 6 (1), 89-94.
*
* Copyright (C) 2019  Ville-Veikko Wettenhovi
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
#include "opencl_functions.h"
#define TYPE 1

// Matrix free Improved Siddon's algorithm
__kernel __attribute__((vec_type_hint(float))) __attribute__((reqd_work_group_size(64, 1, 1)))
void siddon_multi(const float d_epps, const uint d_N, const uint d_Nx, const uint d_Ny, const uint d_Nz, const float d_dz, const float d_dx,
	const float d_dy, const float d_bz, const float d_bx, const float d_by, const float d_bzb, const float d_maxxx, const float d_maxyy,
	const float d_zmax, const float d_NSlices, const uint d_size_x, const ushort d_TotSinos, const uint d_attenuation_correction, 
	const uint d_normalization, const uint d_randoms, const uint d_det_per_ring, const uchar d_raw, const uint d_pRows, const uint d_Nxy, 
	const __global float* d_atten, const __global float* d_norm, __global float* d_Summ, const __global ushort* d_lor, __constant uint* d_pseudos, 
	const __global float* d_x, const __global float* d_y, const __global float* d_zdet, const __global uint* d_xyindex, const __global ushort* d_zindex, 
	const __global ushort* d_L, const __global float* d_Sino, const __global float* d_sc_ra, const __global float* d_OSEM, __global float* d_rhs_OSEM, 
	const uchar no_norm, const ulong m_size) {
	// Get the current global index
	uint idx = get_global_id(0);
	if (idx >= m_size)
		return;
	float xs, xd, ys, yd, zs, zd;
	// Load the next detector index
	// raw list-mode data
	const float local_sino = (d_Sino[idx]);
	if (no_norm == 1u && local_sino == 0.f)
		return;
	if (d_raw) {
		get_detector_coordinates_raw(d_x, d_y, d_zdet, d_L, d_det_per_ring, idx, d_pseudos, d_pRows, &xs, &xd, &ys, &yd, &zs, &zd);
	}
	// Sinogram data
	else {
		get_detector_coordinates(d_xyindex, d_zindex, d_size_x, idx, d_TotSinos, &xs, &xd, &ys, &yd, &zs, &zd, d_x, d_y, d_zdet);
	}
	// Calculate the x, y and z distances of the detector pair
	const float y_diff = (yd - ys);
	const float x_diff = (xd - xs);
	const float z_diff = (zd - zs);
	// Load the number of voxels the LOR traverses (precomputed)
	uint Np = convert_uint(d_lor[idx]);
	uint local_ind = 0u;
	float local_ele = 0.f;
	float jelppi = 0.f;
	float axOSEM = 0.f;
	// If the measurement is on a same ring
	if (fabs(z_diff) < 1e-6f) {
		// Z-coordinate (ring)
		const uint z_loop = convert_uint((zs / d_zmax) * (d_NSlices - 1.f));
		//d_Summ[lo] = y_diff;
		// If the LOR is perpendicular in the y-direction (Siddon cannot be used)
		if (fabs(y_diff) < 1e-6f) {
			if (yd <= d_maxyy && yd >= d_by) {
				float templ_ijk = 0.f;
				uint tempk = 0u;
				perpendicular_elements(d_by, d_dy, d_Ny, yd, d_dx, d_Nx, d_atten, &templ_ijk, &tempk, d_attenuation_correction, z_loop, d_Ny, 1u,
					d_normalization, d_norm, idx);
				// Calculate the next index and store it as well as the probability of emission
				// If measurements are present, calculate the 
				if (local_sino > 0.f) {
					local_ele = templ_ijk;
					local_ind = tempk;
					for (uint ii = 0u; ii < Np; ii++) {
						denominator_multi(local_ele, &axOSEM, &d_OSEM[local_ind++]);
					}
					nominator_multi(&axOSEM, local_sino, d_epps, 1.f, d_randoms, d_sc_ra, idx);
					local_ind = tempk;
					for (uint ii = 0u; ii < Np; ii++) {
						//rhs_multi(local_ele, &axOSEM, &d_rhs_OSEM[local_ind]);
						atomicAdd_g_f(&d_rhs_OSEM[local_ind], (local_ele * axOSEM));
						if (no_norm == 0u)
							atomicAdd_g_f(&d_Summ[local_ind], local_ele);
						local_ind++;
					}
				}
				else {
					local_ele = templ_ijk;
					local_ind = tempk;
					for (uint ii = 0u; ii < Np; ii++) {
						atomicAdd_g_f(&d_Summ[local_ind++], local_ele);
					}
				}
			}
		}
		else if (fabs(x_diff) < 1e-6f) {
			if (xd <= d_maxxx && xd >= d_bx) {
				float templ_ijk = 0.f;
				uint tempk = 0u;
				perpendicular_elements(d_bx, d_dx, d_Nx, xd, d_dy, d_Ny, d_atten, &templ_ijk, &tempk, d_attenuation_correction, z_loop, 1u, d_Nx, 
					d_normalization, d_norm, idx);
				if (local_sino > 0.f) {
					local_ele = templ_ijk;
					local_ind = tempk;
					for (uint ii = 0u; ii < Np; ii++) {
						denominator_multi(local_ele, &axOSEM, &d_OSEM[local_ind]);
						local_ind += d_Ny;
					}
					nominator_multi(&axOSEM, local_sino, d_epps, 1.f, d_randoms, d_sc_ra, idx);
					local_ind = tempk;
					for (uint ii = 0u; ii < Np; ii++) {
						if (no_norm == 0u)
							atomicAdd_g_f(&d_Summ[local_ind], local_ele);
						//rhs_multi(local_ele, &axOSEM, &d_rhs_OSEM[local_ind]);
						atomicAdd_g_f(&d_rhs_OSEM[local_ind], (local_ele * axOSEM));
						local_ind += d_Ny;
					}
				}
				else {
					local_ele = templ_ijk;
					local_ind = tempk;
					for (uint ii = 0u; ii < Np; ii++) {
						atomicAdd_g_f(&d_Summ[local_ind], local_ele);
						local_ind += d_Ny;
					}
				}
			}
		}
		else {
			int tempi = 0, tempj = 0, iu = 0, ju = 0;
			float txu = 0.f, tyu = 0.f, tc = 0.f, tx0 = 0.f, ty0 = 0.f;
			const bool skip = siddon_pre_loop_2D(d_bx, d_by, x_diff, y_diff, d_maxxx, d_maxyy, d_dx, d_dy, d_Nx, d_Ny, &tempi, &tempj, &txu, &tyu, &Np, TYPE,
				ys, xs, yd, xd, &tc, &iu, &ju, &tx0, &ty0);
			const float L = hypot(x_diff, y_diff); //native_sqrt(x_diff*x_diff + y_diff*y_diff);
			float temp = 0.f;
			float tx0_a = tx0, ty0_a = ty0, tc_a = tc;
			int tempi_a = tempi, tempj_a = tempj;
			for (uint ii = 0u; ii < Np; ii++) {
				local_ind = compute_ind(tempj, tempi, convert_int(z_loop), d_Nx, d_Ny, d_N, d_Nx, d_Nxy);
				if (tx0 < ty0) {
					local_ele = compute_element(&tx0, &tc, L, txu, iu, &tempi, &temp);
				}
				else {
					local_ele = compute_element(&ty0, &tc, L, tyu, ju, &tempj, &temp);
				}
				if (d_attenuation_correction == 1u) {
					jelppi += (local_ele * -d_atten[local_ind]);
				}
				if (local_sino > 0.f) {
					denominator_multi(local_ele, &axOSEM, &d_OSEM[local_ind]);
				}
			}
			temp = 1.f / temp;
			if (d_attenuation_correction == 1u)
				temp *= native_exp(jelppi);
			if (d_normalization == 1u)
				temp *= d_norm[idx];
			tc = tc_a;
			tx0 = tx0_a, ty0 = ty0_a;
			tempi = tempi_a, tempj = tempj_a;
			if (local_sino > 0.f) {
				nominator_multi(&axOSEM, local_sino, d_epps, temp, d_randoms, d_sc_ra, idx);
				for (uint ii = 0u; ii < Np; ii++) {
					local_ind = compute_ind(tempj, tempi, convert_int(z_loop), d_Nx, d_Ny, d_N, d_Nx, d_Nxy);
					if (tx0 < ty0) {
						local_ele = compute_element_2nd(&tx0, &tc, L, txu, iu, &tempi, temp);
					}
					else {
						local_ele = compute_element_2nd(&ty0, &tc, L, tyu, ju, &tempj, temp);
					}
					if (no_norm == 0)
						atomicAdd_g_f(&d_Summ[local_ind], local_ele);
					//rhs_multi(local_ele, &axOSEM, &d_rhs_OSEM[local_ind]);
					atomicAdd_g_f(&d_rhs_OSEM[local_ind], (local_ele * axOSEM));
				}
			}
			else {
				for (uint ii = 0u; ii < Np; ii++) {
					local_ind = compute_ind(tempj, tempi, convert_int(z_loop), d_Nx, d_Ny, d_N, d_Nx, d_Nxy);
					if (tx0 < ty0) {
						local_ele = compute_element_2nd(&tx0, &tc, L, txu, iu, &tempi, temp);
					}
					else {
						local_ele = compute_element_2nd(&ty0, &tc, L, tyu, ju, &tempj, temp);
					}
					atomicAdd_g_f(&d_Summ[local_ind], local_ele);
				}
			}
		}
	}
	else {
		if (fabs(y_diff) < 1e-6f) {
			if (yd <= d_maxyy && yd >= d_by) {
				int tempi = 0, tempk = 0, tempj = 0, iu = 0, ku = 0;
				float txu = 0.f, tzu = 0.f, tc = 0.f, tx0 = 0.f, tz0 = 0.f;
				const bool skip = siddon_pre_loop_2D(d_bx, d_bz, x_diff, z_diff, d_maxxx, d_bzb, d_dx, d_dz, d_Nx, d_Nz, &tempi, &tempk, &txu, &tzu, &Np, TYPE,
					zs, xs, zd, xd, &tc, &iu, &ku, &tx0, &tz0);
				const float L = hypot(x_diff, z_diff);//native_sqrt((x_diff*x_diff + z_diff*z_diff));
				tempj = perpendicular_start(d_by, yd, d_dy, d_Ny);
				tempj *= convert_int(d_Nx);
				float temp = 0.f;
				float tx0_a = tx0, tz0_a = tz0, tc_a = tc;
				int tempi_a = tempi, tempj_a = tempj, tempk_a = tempk;
				for (uint ii = 0u; ii < Np; ii++) {
					local_ind = compute_ind(tempj, tempi, tempk, d_Nx, d_Ny, d_N, 1u, d_Nxy);
					if (tx0 < tz0) {
						local_ele = compute_element(&tx0, &tc, L, txu, iu, &tempi, &temp);
					}
					else {
						local_ele = compute_element(&tz0, &tc, L, tzu, ku, &tempk, &temp);
					}
					if (d_attenuation_correction == 1u) {
						jelppi += (local_ele * -d_atten[local_ind]);
					}
					if (local_sino > 0.f) {
						denominator_multi(local_ele, &axOSEM, &d_OSEM[local_ind]);
					}
				}
				temp = 1.f / temp;
				if (d_attenuation_correction == 1u)
					temp *= native_exp(jelppi);
				if (d_normalization == 1u)
					temp *= d_norm[idx];
				tc = tc_a;
				tx0 = tx0_a, tz0 = tz0_a;
				tempi = tempi_a, tempj = tempj_a, tempk = tempk_a;
				if (local_sino > 0.f) {
					nominator_multi(&axOSEM, local_sino, d_epps, temp, d_randoms, d_sc_ra, idx);
					for (uint ii = 0u; ii < Np; ii++) {
						local_ind = compute_ind(tempj, tempi, tempk, d_Nx, d_Ny, d_N, 1u, d_Nxy);
						if (tx0 < tz0) {
							local_ele = compute_element_2nd(&tx0, &tc, L, txu, iu, &tempi, temp);
						}
						else {
							local_ele = compute_element_2nd(&tz0, &tc, L, tzu, ku, &tempk, temp);
						}
						if (no_norm == 0)
							atomicAdd_g_f(&d_Summ[local_ind], local_ele);
						atomicAdd_g_f(&d_rhs_OSEM[local_ind], (local_ele * axOSEM));
						//rhs_multi(local_ele, &axOSEM, &d_rhs_OSEM[local_ind]);
					}
				}
				else {
					for (uint ii = 0u; ii < Np; ii++) {
						local_ind = compute_ind(tempj, tempi, tempk, d_Nx, d_Ny, d_N, 1u, d_Nxy);
						if (tx0 < tz0) {
							local_ele = compute_element_2nd(&tx0, &tc, L, txu, iu, &tempi, temp);
						}
						else {
							local_ele = compute_element_2nd(&tz0, &tc, L, tzu, ku, &tempk, temp);
						}
						atomicAdd_g_f(&d_Summ[local_ind], local_ele);
					}
				}
			}
		}
		else if (fabs(x_diff) < 1e-6f) {
			if (xd <= d_maxxx && xd >= d_bx) {
				int tempi = 0, tempk = 0, tempj = 0, ju = 0, ku = 0;
				float tyu = 0.f, tzu = 0.f, tc = 0.f, ty0 = 0.f, tz0 = 0.f;
				const bool skip = siddon_pre_loop_2D(d_by, d_bz, y_diff, z_diff, d_maxyy, d_bzb, d_dy, d_dz, d_Ny, d_Nz, &tempj, &tempk, &tyu, &tzu, &Np, TYPE,
					zs, ys, zd, yd, &tc, &ju, &ku, &ty0, &tz0);
				const float L = hypot(y_diff, z_diff);//native_sqrt((y_diff*y_diff + z_diff*z_diff));
				float temp = 0.f;
				tempi = perpendicular_start(d_bx, xd, d_dx, d_Nx);
				float tz0_a = tz0, ty0_a = ty0, tc_a = tc;
				int tempi_a = tempi, tempj_a = tempj, tempk_a = tempk;
				for (uint ii = 0u; ii < Np; ii++) {
					local_ind = compute_ind(tempj, tempi, tempk, d_Nx, d_Ny, d_N, d_Nx, d_Nxy);
					if (tz0 < ty0) {
						local_ele = compute_element(&tz0, &tc, L, tzu, ku, &tempk, &temp);
					}
					else {
						local_ele = compute_element(&ty0, &tc, L, tyu, ju, &tempj, &temp);
					}
					if (d_attenuation_correction == 1u) {
						jelppi += (local_ele * -d_atten[local_ind]);
					}
					if (local_sino > 0.f) {
						denominator_multi(local_ele, &axOSEM, &d_OSEM[local_ind]);
					}
				}
				temp = 1.f / temp;
				if (d_attenuation_correction == 1u)
					temp *= native_exp(jelppi);
				if (d_normalization == 1u)
					temp *= d_norm[idx];
				tc = tc_a;
				tz0 = tz0_a, ty0 = ty0_a;
				tempi = tempi_a, tempj = tempj_a, tempk = tempk_a;
				if (local_sino > 0.f) {
					nominator_multi(&axOSEM, local_sino, d_epps, temp, d_randoms, d_sc_ra, idx);
					for (uint ii = 0u; ii < Np; ii++) {
						local_ind = compute_ind(tempj, tempi, tempk, d_Nx, d_Ny, d_N, d_Nx, d_Nxy);
						if (tz0 < ty0) {
							local_ele = compute_element_2nd(&tz0, &tc, L, tzu, ku, &tempk, temp);
						}
						else {
							local_ele = compute_element_2nd(&ty0, &tc, L, tyu, ju, &tempj, temp);
						}
						if (no_norm == 0)
							atomicAdd_g_f(&d_Summ[local_ind], local_ele);
						atomicAdd_g_f(&d_rhs_OSEM[local_ind], (local_ele * axOSEM));
						//rhs_multi(local_ele, &axOSEM, &d_rhs_OSEM[local_ind]);
					}
				}
				else {
					for (uint ii = 0u; ii < Np; ii++) {
						local_ind = compute_ind(tempj, tempi, tempk, d_Nx, d_Ny, d_N, d_Nx, d_Nxy);
						if (tz0 < ty0) {
							local_ele = compute_element_2nd(&tz0, &tc, L, tzu, ku, &tempk, temp);
						}
						else {
							local_ele = compute_element_2nd(&ty0, &tc, L, tyu, ju, &tempj, temp);
						}
						atomicAdd_g_f(&d_Summ[local_ind], local_ele);
					}
				}
			}
		}
		else {
			int tempi = 0, tempj = 0, tempk = 0, iu = 0, ju = 0, ku = 0;
			float txu = 0.f, tyu = 0.f, tzu = 0.f, tc = 0.f, tx0 = 0.f, ty0 = 0.f, tz0 = 0.f;
			const bool skip = siddon_pre_loop_3D(d_bx, d_by, d_bz, x_diff, y_diff, z_diff, d_maxxx, d_maxyy, d_bzb, d_dx, d_dy, d_dz, d_Nx, d_Ny, d_Nz, 
				&tempi, &tempj, &tempk, &tyu, &txu, &tzu, &Np, TYPE, ys, xs, yd, xd, zs, zd, &tc, &iu, &ju, &ku, &tx0, &ty0, &tz0);
			const float L = native_sqrt(x_diff * x_diff + z_diff * z_diff + y_diff * y_diff);
			float temp = 0.f;
			const float tx0_a = tx0, ty0_a = ty0, tz0_a = tz0, tc_a = tc;
			const int tempi_a = tempi, tempj_a = tempj, tempk_a = tempk;
			for (uint ii = 0u; ii < Np; ii++) {
				local_ind = compute_ind(tempj, tempi, tempk, d_Nx, d_Ny, d_N, d_Nx, d_Nxy);
				if (tz0 < ty0 && tz0 < tx0) {
					local_ele = compute_element(&tz0, &tc, L, tzu, ku, &tempk, &temp);
				}
				else if (ty0 < tx0) {
					local_ele = compute_element(&ty0, &tc, L, tyu, ju, &tempj, &temp);
				}
				else {
					local_ele = compute_element(&tx0, &tc, L, txu, iu, &tempi, &temp);
				}
				if (d_attenuation_correction == 1u) {
					jelppi += (local_ele * -d_atten[local_ind]);
				}
				if (local_sino > 0.f) {
					denominator_multi(local_ele, &axOSEM, &d_OSEM[local_ind]);
				}
			}
			temp = 1.f / temp;
			if (d_attenuation_correction == 1u)
				temp *= native_exp(jelppi);
			if (d_normalization == 1u)
				temp *= d_norm[idx];
			tc = tc_a;
			tx0 = tx0_a, ty0 = ty0_a, tz0 = tz0_a;
			tempi = tempi_a, tempj = tempj_a, tempk = tempk_a;
			if (local_sino > 0.f) {
				nominator_multi(&axOSEM, local_sino, d_epps, temp, d_randoms, d_sc_ra, idx);
				for (uint ii = 0u; ii < Np; ii++) {
					local_ind = compute_ind(tempj, tempi, tempk, d_Nx, d_Ny, d_N, d_Nx, d_Nxy);
					if (tz0 < ty0 && tz0 < tx0) {
						local_ele = compute_element_2nd(&tz0, &tc, L, tzu, ku, &tempk, temp);
					}
					else if (ty0 < tx0) {
						local_ele = compute_element_2nd(&ty0, &tc, L, tyu, ju, &tempj, temp);
					}
					else {
						local_ele = compute_element_2nd(&tx0, &tc, L, txu, iu, &tempi, temp);
					}
					if (no_norm == 0)
						atomicAdd_g_f(&d_Summ[local_ind], local_ele);
					atomicAdd_g_f(&d_rhs_OSEM[local_ind], (local_ele * axOSEM));
					//rhs_multi(local_ele, &axOSEM, &d_rhs_OSEM[local_ind]);
				}
			}
			else {
				for (uint ii = 0u; ii < Np; ii++) {
					local_ind = compute_ind(tempj, tempi, tempk, d_Nx, d_Ny, d_N, d_Nx, d_Nxy);
					if (tz0 < ty0 && tz0 < tx0) {
						local_ele = compute_element_2nd(&tz0, &tc, L, tzu, ku, &tempk, temp);
					}
					else if (ty0 < tx0) {
						local_ele = compute_element_2nd(&ty0, &tc, L, tyu, ju, &tempj, temp);
					}
					else {
						local_ele = compute_element_2nd(&tx0, &tc, L, txu, iu, &tempi, temp);
					}
					atomicAdd_g_f(&d_Summ[local_ind], local_ele);
				}
			}
		}

	}
}


__kernel void summa(const __global float* d_Summ_device, __global float* d_Summ_local, const __global float* d_rhs_device, __global float* d_rhs_local,
	const uint im_dim, const uchar no_norm) {

	uint gid = get_global_id(0);

	for (uint i = gid; i < im_dim; i += get_global_size(0)) {
		if (no_norm == 0u)
			d_Summ_local[i] += d_Summ_device[i];
		d_rhs_local[i] += d_rhs_device[i];
	}
}

__kernel void mlem(const __global float* d_Summ, const __global float* d_rhs, __global float* d_mlem, const uint im_dim, const float d_epps) {

	uint gid = get_global_id(0);

	for (uint i = gid; i < im_dim; i += get_global_size(0)) {
		if (d_rhs[i] != 0.f) {
			if (d_Summ[i] == 0.f)
				d_mlem[i] = d_mlem[i] / d_epps * d_rhs[i];
			else
				d_mlem[i] = d_mlem[i] / d_Summ[i] * d_rhs[i];
		}
		else {
			if (d_Summ[i] != 0.f)
				d_mlem[i] = d_mlem[i] / d_Summ[i] * d_epps;
		}
	}
}