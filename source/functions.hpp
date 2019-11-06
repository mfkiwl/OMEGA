/**************************************************************************
* Header for matrix-free OpenCL functions
*
* Copyright(C) 2019  Ville - Veikko Wettenhovi
*
* This program is free software : you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
***************************************************************************/
#pragma once
#include "opencl_error.hpp"
#include "precomp.h"
#include <af/opencl.h>
#include <arrayfire.h>
#include <algorithm>
#include <vector>

#pragma pack(1)

//#undef max
//#undef min

constexpr auto TH = 100000000000.f;

// Struct for the TV-prior
typedef struct {
	af::array s1, s2, s3, s4, s5, s6, s7, s8, s9, reference_image, APLSReference;
	bool TV_use_anatomical;
	float tau, TVsmoothing, T, C, eta, APLSsmoothing, TGVAlpha, TGVBeta;
	uint32_t TVtype = 0;
	uint32_t NiterTGV;
} TVdata;

// Struct for the various estimates
// Default values are scalars (needed for OpenCL kernel)
typedef struct {
	af::array OSEM, MLEM, RAMLA, MRAMLA, ROSEM, RBI, DRAMA, COSEM, ECOSEM, ACOSEM, 
		MRP_OSEM, MRP_MLEM, MRP_MBSREM, MRP_BSREM, MRP_ROSEM, MRP_RBI, MRP_COSEM,
		Quad_OSEM, Quad_MLEM, Quad_MBSREM, Quad_BSREM, Quad_ROSEM, Quad_RBI, Quad_COSEM, 
		L_OSEM, L_MLEM, L_MBSREM, L_BSREM, L_ROSEM, L_RBI, L_COSEM,
		FMH_OSEM, FMH_MLEM, FMH_MBSREM, FMH_BSREM, FMH_ROSEM, FMH_RBI, FMH_COSEM, 
		Weighted_OSEM, Weighted_MLEM, Weighted_MBSREM, Weighted_BSREM, Weighted_ROSEM, Weighted_RBI, Weighted_COSEM, 
		TV_OSEM, TV_MLEM, TV_MBSREM, TV_BSREM, TV_ROSEM, TV_RBI, TV_COSEM, 
		AD_OSEM, AD_MLEM, AD_MBSREM, AD_BSREM, AD_ROSEM, AD_RBI, AD_COSEM, 
		APLS_OSEM, APLS_MLEM, APLS_MBSREM, APLS_BSREM, APLS_ROSEM, APLS_RBI, APLS_COSEM, 
		TGV_OSEM, TGV_MLEM, TGV_MBSREM, TGV_BSREM, TGV_ROSEM, TGV_RBI, TGV_COSEM,
		NLM_OSEM, NLM_MLEM, NLM_MBSREM, NLM_BSREM, NLM_ROSEM, NLM_RBI, NLM_COSEM,
		custom_OSEM, custom_MLEM, custom_MBSREM, custom_BSREM, custom_ROSEM, custom_RBI, custom_COSEM;
	af::array C_co = af::constant(0.f, 1, 1), C_aco = af::constant(0.f, 1, 1), C_osl = af::constant(0.f, 1, 1);
	af::array im_mlem, rhs_mlem, im_os, rhs_os;
} AF_im_vectors;


// Struct for the various estimates used in OpenCL kernels
typedef struct _OpenCL_im_vectors {
	cl_mem* d_im_mlem, * d_rhs_mlem, * d_im_os, * d_rhs_os;
} OpenCL_im_vectors;

// Struct for the regularization parameters
typedef struct {
	float MRP_OSEM, MRP_MLEM, MRP_MBSREM, MRP_BSREM, MRP_ROSEM, MRP_RBI, MRP_COSEM,
		Quad_OSEM, Quad_MLEM, Quad_MBSREM, Quad_BSREM, Quad_ROSEM, Quad_RBI, Quad_COSEM,
		L_OSEM, L_MLEM, L_MBSREM, L_BSREM, L_ROSEM, L_RBI, L_COSEM,
		FMH_OSEM, FMH_MLEM, FMH_MBSREM, FMH_BSREM, FMH_ROSEM, FMH_RBI, FMH_COSEM,
		Weighted_OSEM, Weighted_MLEM, Weighted_MBSREM, Weighted_BSREM, Weighted_ROSEM, Weighted_RBI, Weighted_COSEM,
		TV_OSEM, TV_MLEM, TV_MBSREM, TV_BSREM, TV_ROSEM, TV_RBI, TV_COSEM,
		AD_OSEM, AD_MLEM, AD_MBSREM, AD_BSREM, AD_ROSEM, AD_RBI, AD_COSEM,
		APLS_OSEM, APLS_MLEM, APLS_MBSREM, APLS_BSREM, APLS_ROSEM, APLS_RBI, APLS_COSEM,
		TGV_OSEM, TGV_MLEM, TGV_MBSREM, TGV_BSREM, TGV_ROSEM, TGV_RBI, TGV_COSEM,
		NLM_OSEM, NLM_MLEM, NLM_MBSREM, NLM_BSREM, NLM_ROSEM, NLM_RBI, NLM_COSEM,
		custom_OSEM, custom_MLEM, custom_MBSREM, custom_BSREM, custom_ROSEM, custom_RBI, custom_COSEM;
} Beta;

// Struct for various parameters, mainly various weights and coefficients
typedef struct {
	af::array tr_offsets, weights_quad, fmh_weights, a_L, weighted_weights, UU, Amin, D, ACOSEM_rhs = af::constant(0.f, 1, 1);
	af::array dU_OSEM, dU_MLEM, dU_BSREM, dU_MBSREM, dU_ROSEM, dU_RBI, dU_COSEM;
	af::array NLM_ref;
	float *lambda, *lambda_MBSREM, *lambda_BSREM, *lambda_ROSEM, *lambda_DRAMA, h_ACOSEM = 1.f, TimeStepAD, KAD, w_sum;// , epsilon_mramla = 0.f;
	float epsilon_mramla = 0.f, U, NLM_gauss = 1.f, h2;
	uint32_t alku_fmh, mean_type;
	af_flux_function FluxType;
	af_diffusion_eq DiffusionType;
	uint32_t Ndx, Ndy, Ndz, NiterAD, dimmu, inffi, Nlx, Nly, Nlz;
	bool med_no_norm = false, MBSREM_prepass = false, NLM_MRP = false, NLTV = false, NLM_anatomical = false;
} Weighting;

// Struct for boolean operators indicating whether a certain method is selected
typedef struct {
	bool MLEM, OSEM, MRAMLA, RAMLA, ROSEM, RBI, DRAMA, COSEM, ECOSEM, ACOSEM;
	bool MRP, Quad, L, FMH, WeightedMean, TV, AD, APLS, TGV, NLM;
	bool OSLMLEM, OSLOSEM, MBSREM, BSREM, ROSEMMAP, RBIMAP;
	bool MAP;
	bool CUSTOM = false;
	uint32_t OSLCOSEM;
} RecMethods;

// Struct for boolean operators indicating whether a certain method is selected (OpenCL)
typedef struct _RecMethodsOpenCL {
	cl_char MLEM, OSEM, MRAMLA, RAMLA, ROSEM, RBI, DRAMA, COSEM, ECOSEM, ACOSEM;
	cl_char MRP, Quad, L, FMH, WeightedMean, TV, AD, APLS, TGV, NLM;
	cl_char OSLMLEM, OSLOSEM, MBSREM, BSREM, ROSEMMAP, RBIMAP;
	cl_char OSLCOSEM;
} RecMethodsOpenCL;

// MATLAB output arrays
typedef struct {
	mxArray* mlem, * osem, * ramla, * ramlaM, * rosem, * rbi, * drama, * cosem, * ecosem, * acosem,
		* mrp_mlem, * quad_mlem, * L_mlem, * fmh_mlem, * weighted_mlem, * TV_mlem, * AD_mlem, * APLS_mlem, * TGV_mlem, * NLM_mlem,
		* mrp_osem, * quad_osem, * L_osem, * fmh_osem, * weighted_osem, * TV_osem, * AD_osem, * APLS_osem, * TGV_osem, * NLM_osem,
		* mrp_bsrem, * quad_bsrem, * L_bsrem, * fmh_bsrem, * weighted_bsrem, * TV_bsrem, * AD_bsrem, * APLS_bsrem, * TGV_bsrem, * NLM_bsrem,
		* mrp_mbsrem, * quad_mbsrem, * L_mbsrem, * fmh_mbsrem, * weighted_mbsrem, * TV_mbsrem, * AD_mbsrem, * APLS_mbsrem, * TGV_mbsrem, * NLM_mbsrem,
		* mrp_rosem, * quad_rosem, * L_rosem, * fmh_rosem, * weighted_rosem, * TV_rosem, * AD_rosem, * APLS_rosem, * TGV_rosem, * NLM_rosem,
		* mrp_rbi, * quad_rbi, * L_rbi, * fmh_rbi, * weighted_rbi, * TV_rbi, * AD_rbi, * APLS_rbi, * TGV_rbi, * NLM_rbi,
		* mrp_cosem, * quad_cosem, * L_cosem, * fmh_cosem, * weighted_cosem, * TV_cosem, * AD_cosem, * APLS_cosem, * TGV_cosem, * NLM_cosem,
		* custom_osem, * custom_mlem, * custom_bsrem, * custom_mbsrem, * custom_rosem, * custom_rbi, * custom_cosem;
	mxArray* c_osl_custom, *D_custom;
	float* ele_os, * ele_ml, * ele_ramla, * ele_ramlaM, * ele_rosem, * ele_rbi, * ele_drama, * ele_cosem, * ele_ecosem, * ele_acosem,
		* ele_mrp_mlem, * ele_quad_mlem, * ele_L_mlem, * ele_fmh_mlem, * ele_weighted_mlem, * ele_TV_mlem, * ele_AD_mlem, * ele_APLS_mlem, * ele_TGV_mlem, * ele_NLM_mlem,
		* ele_mrp_osem, * ele_quad_osem, * ele_L_osem, * ele_fmh_osem, * ele_weighted_osem, * ele_TV_osem, * ele_AD_osem, * ele_APLS_osem, * ele_TGV_osem, * ele_NLM_osem,
		* ele_mrp_bsrem, * ele_quad_bsrem, * ele_L_bsrem, * ele_fmh_bsrem, * ele_weighted_bsrem, * ele_TV_bsrem, * ele_AD_bsrem, * ele_TGV_bsrem, * ele_APLS_bsrem, * ele_NLM_bsrem,
		* ele_mrp_mbsrem, * ele_quad_mbsrem, * ele_L_mbsrem, * ele_fmh_mbsrem, * ele_weighted_mbsrem, * ele_TV_mbsrem, * ele_AD_mbsrem, * ele_TGV_mbsrem, * ele_APLS_mbsrem, * ele_NLM_mbsrem,
		* ele_mrp_rosem, * ele_quad_rosem, * ele_L_rosem, * ele_fmh_rosem, * ele_weighted_rosem, * ele_TV_rosem, * ele_AD_rosem, * ele_TGV_rosem, * ele_APLS_rosem, * ele_NLM_rosem,
		* ele_mrp_rbi, * ele_quad_rbi, * ele_L_rbi, * ele_fmh_rbi, * ele_weighted_rbi, * ele_TV_rbi, * ele_AD_rbi, * ele_TGV_rbi, * ele_APLS_rbi, * ele_NLM_rbi,
		* ele_mrp_cosem, * ele_quad_cosem, * ele_L_cosem, * ele_fmh_cosem, * ele_weighted_cosem, * ele_TV_cosem, * ele_AD_cosem, * ele_TGV_cosem, * ele_APLS_cosem, * ele_NLM_cosem,
		* ele_custom_osem, * ele_custom_mlem, * ele_custom_bsrem, * ele_custom_mbsrem, * ele_custom_rosem, * ele_custom_rbi, * ele_custom_cosem;
	float* ele_c_osl_custom, *ele_D_custom;
} matlabArrays;

// Function for loading the data and forming the initial data variables (initial image estimates, etc.)
void form_data_variables(AF_im_vectors &vec, Beta &beta, Weighting &w_vec, const mxArray* options, const uint32_t Nx, const uint32_t Ny,
	const uint32_t Nz, const uint32_t Niter, const af::array &x0, const uint32_t im_dim, const size_t koko_l, const RecMethods &MethodList, TVdata &data, 
	const uint32_t subsets, const uint32_t osa_iter0);

// Same as above, but for the custom prior implementation
void form_data_variables_custom(AF_im_vectors &vec, Beta &beta, Weighting &w_vec, const mxArray* options, const uint32_t Nx, const uint32_t Ny,
	const uint32_t Nz, const uint32_t Niter, const uint32_t im_dim, const size_t koko_l, const RecMethods &MethodList, TVdata &data, const uint32_t subsets, 
	const uint32_t iter);

// Get the reconstruction methods used
void get_rec_methods(const mxArray *options, RecMethods &MethodList);

// Initialize the OpenCL kernel inputs
void initialize_opencl_inputs(AF_im_vectors & vec, OpenCL_im_vectors &vec_opencl, const RecMethods &MethodList, const bool mlem, const bool osem,
	const cl_context af_context, const cl_command_queue af_queue, const uint32_t im_dim);

// Update the OpenCL inputs for the current iteration/subset
void update_opencl_inputs(AF_im_vectors & vec, OpenCL_im_vectors &vec_opencl, const bool mlem, const uint32_t im_dim, const uint32_t n_rekos,
	const uint32_t n_rekos_mlem, const RecMethods MethodList, const bool atomic_64bit);

// Transfer memory control back to ArrayFire
void unlock_AF_im_vectors(AF_im_vectors & vec, const RecMethods &MethodList, const bool finished, const bool mlem, const bool osem, const uint32_t osa_iter);

// For OpenCL
void OpenCLRecMethods(const RecMethods &MethodList, RecMethodsOpenCL &MethodListOpenCL);

// MATLAB output
void create_matlab_output(matlabArrays &ArrayList, const mwSize *dimmi, const RecMethods &MethodList, const uint32_t dim_n);

// Transfer device data back to host MATLAB cell
void device_to_host_cell(matlabArrays &ArrayList, const RecMethods &MethodList, AF_im_vectors & vec, uint32_t &oo, mxArray *cell, Weighting& w_vec);

// Same as above, but for custom prior
void device_to_host_cell_custom(matlabArrays &ArrayList, const RecMethods &MethodList, AF_im_vectors & vec, uint32_t &oo, mxArray *cell);

// Compute the epsilon value for the MBSREM/MRAMLA
float MBSREM_epsilon(const af::array &Sino, const float epps, const uint32_t randoms_correction, const af::array& randoms, const af::array& D);

// Save the OpenCL program binary
cl_int SaveProgramBinary(const bool verbose, const char* k_path, cl_context af_context, cl_device_id af_device_id, const char* fileName, cl_program &program, 
	bool& atomic_64bit, const uint32_t device, const char* header_directory, const bool force_build);

// Load the OpenCL binary and create an OpenCL program from it
cl_int CreateProgramFromBinary(cl_context af_context, cl_device_id af_device_id, FILE *fp, cl_program &program);

cl_int createKernels(cl_kernel &kernel_ml, cl_kernel &kernel, cl_kernel &kernel_mramla, const bool osem_bool, const cl_program &program, 
	const RecMethods MethodList, const Weighting w_vec, const uint32_t projector_type, const bool mlem_bool, const bool precompute);

cl_int createAndWriteBuffers(cl_mem& d_x, cl_mem& d_y, cl_mem& d_z, std::vector<cl_mem>& d_lor, std::vector<cl_mem>& d_L, std::vector<cl_mem>& d_zindex,
	std::vector<cl_mem>& d_xyindex, std::vector<cl_mem>& d_Sino, std::vector<cl_mem>& d_sc_ra, const uint32_t size_x, const size_t size_z,
	const uint32_t TotSinos, const size_t size_atten, const size_t size_norm, const uint32_t prows, std::vector<size_t>& length, const float* x, const float* y,
	const float* z_det, const uint32_t* xy_index, const uint16_t* z_index, const uint16_t* lor1, const uint16_t* L, const float* Sin, const uint8_t raw,
	cl_context& af_context, const uint32_t subsets, const uint32_t* pituus, const float* atten, const float* norm, const uint32_t* pseudos,
	cl_command_queue& af_queue, cl_mem& d_atten, cl_mem& d_norm, cl_mem& d_pseudos, cl_mem& d_xcenter, cl_mem& d_ycenter, cl_mem& d_zcenter,
	const float* x_center, const float* y_center, const float* z_center, const size_t size_center_x, const size_t size_center_y, const size_t size_center_z,
	const size_t size_of_x, const bool atomic_64bit, const bool randoms_correction, const mxArray* sc_ra, const bool precompute, cl_mem& d_lor_mlem,
	cl_mem& d_L_mlem, cl_mem& d_zindex_mlem, cl_mem& d_xyindex_mlem, cl_mem& d_Sino_mlem, cl_mem& d_sc_ra_mlem, cl_mem& d_reko_type, const bool osem_bool, 
	const bool mlem_bool, const size_t koko, const uint8_t* reko_type, const uint32_t n_rekos);

// Prepass phase for MRAMLA, MBSREM, COSEM, ACOSEM, ECOSEM, RBI
void MRAMLA_prepass(const uint32_t subsets, const uint32_t im_dim, const uint32_t* pituus, const std::vector<cl_mem> &lor, const std::vector<cl_mem> &zindex,
	const std::vector<cl_mem> &xindex, cl_program program, const cl_command_queue af_queue, const cl_context af_context, Weighting& w_vec, 
	std::vector<af::array>& Summ, const std::vector<cl_mem> &d_Sino, const size_t koko_l, const af::array& cosem, af::array& C_co, 
	af::array& C_aco, af::array& C_osl, const uint32_t alku, cl_kernel &kernel_mramla, const std::vector<cl_mem> &L, const uint8_t raw, 
	const RecMethodsOpenCL MethodListOpenCL, const std::vector<size_t> length, const bool atomic_64bit, const cl_uchar compute_norm_matrix, 
	const std::vector<cl_mem>& d_sc_ra, cl_uint kernelInd_MRAMLA, af::array& E);

// Batch functions (scalar and vector or vector and matrix)
af::array batchMinus(const af::array &lhs, const af::array &rhs);

af::array batchPlus(const af::array &lhs, const af::array &rhs);

af::array batchMul(const af::array &lhs, const af::array &rhs);

af::array batchDiv(const af::array &lhs, const af::array &rhs);

af::array batchNotEqual(const af::array &lhs, const af::array &rhs);

// Create a zero-padded image
af::array padding(const af::array& im, const uint32_t Nx, const uint32_t Ny, const uint32_t Nz, const uint32_t Ndx, const uint32_t Ndy, const uint32_t Ndz, 
	const bool zero_pad = false);

// Reconstruction methods
af::array MLEM(const af::array &im, const af::array &Summ, const af::array &rhs);

af::array OSL_MLEM(const af::array &im, const af::array &Summ, const af::array &rhs, const af::array &dU, const float beta);

af::array OSEM(const af::array &im, const af::array &Summ, const af::array &rhs);

af::array OSL_OSEM(const af::array &im, const af::array &Summ, const af::array &rhs, const af::array &dU, const float beta);

af::array MBSREM(const af::array & im, const af::array & rhs, const float U, const af::array & pj3, const float* lam, const uint32_t iter, const uint32_t im_dim,
	const float beta, const af::array &dU, const af::array & Summ, const float epps);

af::array BSREM(const af::array &im, const af::array &rhs, const float *lam, const uint32_t iter);

//af::array COSEM(const af::array &im, const af::array &C_co, const af::array &D);

af::array ECOSEM(const af::array &im, const af::array &D, const af::array &OSEM_apu, const af::array &COSEM_apu, const float epps);

//af::array ACOSEM(const af::array & im, const af::array & C_aco, const af::array & D, const float h);

af::array ROSEM(const af::array &im, const af::array &Summ, const af::array &rhs, const float *lam, const uint32_t iter);

af::array RBI(const af::array & im, const af::array & Summ, const af::array & rhs, const af::array &D, const float beta, const af::array & dU);

af::array DRAMA(const af::array &im, const af::array &Summ, const af::array &rhs, const float *lam, const uint32_t iter, const uint32_t sub_iter,
	const uint32_t subsets);

af::array BSREM_MAP(const af::array &im, const float *lam, const uint32_t iter, const float beta, const af::array &dU, const float epps);

af::array OSL_COSEM(const af::array &im, const af::array &C_co, const af::array &D, const float h, const uint32_t COSEM_TYPE, const af::array &dU, const float beta);

// Priors
af::array MRP(const af::array &im, const uint32_t medx, const uint32_t medy, const uint32_t medz, const uint32_t Nx, const uint32_t Ny, const uint32_t Nz, 
	const float epps, const af::array &offsets, const bool med_no_norm, const uint32_t im_dim);

af::array Quadratic_prior(const af::array &im, const uint32_t Ndx, const uint32_t Ndy, const uint32_t Ndz, const uint32_t Nx, const uint32_t Ny, 
	const uint32_t Nz, const uint32_t inffi, const af::array &offsets, const af::array &weights_quad, const uint32_t im_dim);

af::array FMH(const af::array &im, const uint32_t Ndx, const uint32_t Ndy, const uint32_t Ndz, const uint32_t Nx, const uint32_t Ny, const uint32_t Nz, 
	const float epps, const uint32_t inffi, const af::array &offsets, const af::array &fmh_weights, const bool med_no_norm, const uint32_t alku_fmh, 
	const uint32_t im_dim);

af::array L_filter(const af::array &im, const uint32_t Ndx, const uint32_t Ndy, const uint32_t Ndz, const uint32_t Nx, const uint32_t Ny, const uint32_t Nz, 
	const float epps, const af::array &offsets, const af::array &a_L, const bool med_no_norm, const uint32_t im_dim);

af::array Weighted_mean(const af::array &im, const uint32_t Ndx, const uint32_t Ndy, const uint32_t Ndz, const uint32_t Nx, const uint32_t Ny, 
	const uint32_t Nz, const float epps, const af::array &offsets, const af::array &weighted_weights, const bool med_no_norm, const uint32_t im_dim, 
	const uint32_t mean_type, const float w_sum);

af::array AD(const af::array &im, const uint32_t Nx, const uint32_t Ny, const uint32_t Nz, const float epps, const float TimeStepAD, const float KAD, 
	const uint32_t NiterAD, const af_flux_function FluxType, const af_diffusion_eq DiffusionType, const bool med_no_norm);

af::array TVprior(const uint32_t Nx, const uint32_t Ny, const uint32_t Nz, const TVdata &S, const af::array& im, const float epps, const uint32_t TVtype, 
	const Weighting & w_vec, const af::array& offsets);

af::array TGV(const af::array &im, const uint32_t Nx, const uint32_t Ny, const uint32_t Nz, const uint32_t maxits, const float alpha, const float beta);

af::array NLM(const af::array& im, const uint32_t Ndx, const uint32_t Ndy, const uint32_t Ndz, const uint32_t Nlx, const uint32_t Nly, const uint32_t Nlz,
	const float h2, const float epps, const uint32_t Nx, const uint32_t Ny, const uint32_t Nz, const bool NLM_anatomical, const float NLM_gauss,
	const bool NLTV, const bool NLM_MRP, const af::array& NLM_ref);

void reconstruction_AF_matrixfree(const size_t koko, const uint16_t* lor1, const float* z_det, const float* x, const float* y, const mxArray* Sin, 
	const mxArray* sc_ra, const uint32_t Nx, const uint32_t Ny, const uint32_t Nz, const uint32_t Niter, const mxArray* options, const float dx, 
	const float dy, const float dz, const float bx, const float by, const float bz, const float bzb, const float maxxx, const float maxyy, const float zmax, 
	const float NSlices, const uint32_t* pituus, const uint32_t* xy_index, const uint16_t* z_index, const uint32_t size_x, const uint32_t TotSinos, 
	mxArray* cell, const mwSize* dimmi, const bool verbose, const uint32_t randoms_correction, const uint32_t attenuation_correction, 
	const uint32_t normalization, const float* atten, const size_t size_atten, const float* norm, const size_t size_norm, const uint32_t subsets, 
	const float epps, const char* k_path, const uint32_t Nt,	const uint32_t* pseudos, const uint32_t det_per_ring, const uint32_t prows, 
	const uint16_t* L, const uint8_t raw, const size_t size_z, const bool osem_bool, const char* fileName, const bool force_build, const float tube_width, 
	const float crystal_size_z, const float* x_center, const float* y_center, const float* z_center, const size_t size_center_x, const size_t size_center_y, 
	const size_t size_of_x, const size_t size_center_z, const uint32_t projector_type, const char* header_directory, const bool precompute, 
	const uint32_t device, const int32_t dec, const uint16_t n_rays, const float cr_pz, const bool use_64bit_atomics, uint32_t n_rekos, 
	const uint32_t n_rekos_mlem, const uint8_t* reko_type);

//void reconstruction_custom_matrixfree(const size_t koko, const uint16_t* lor1, const float* z_det, const float* x, const float* y, const float* Sin, const uint32_t Nx, const uint32_t Ny,
//	const uint32_t Nz, const uint32_t iter, const mxArray* options, const float dx, const float dy, const float dz, const float bx, const float by, const float bz,
//	const float bzb, const float maxxx, const float maxyy, const float zmax, const float NSlices, uint32_t* pituus, const size_t koko_l,
//	const uint32_t* xy_index, const uint16_t* z_index, const uint32_t size_x, const uint32_t TotSinos, mxArray *cell, const mwSize dimmi, const bool verbose,
//	const uint32_t attenuation_correction, const uint32_t normalization, const float* atten, const size_t size_atten, const float* norm, const size_t size_norm, const uint32_t subsets, const float epps, const uint8_t* rekot,
//	const char* k_path, const size_t size_rekot, const uint32_t* pseudos, const uint32_t det_per_ring, const uint32_t prows, const uint16_t* L,
//	const uint8_t raw, const size_t size_z, const bool osem_bool, const char* fileName, const bool force_build, const uint32_t osa_iter, const uint32_t tt,
//	const uint32_t n_subsets, const bool mlem_bool, const float tube_width, const float crystal_size_z, const float* x_center, const float* y_center, const float* z_center, const size_t size_center_x, 
//	const size_t size_center_y, const size_t size_center_z, const size_t size_of_x, const uint32_t projector_type, const uint32_t device);

void find_LORs(uint16_t* lor, const float* z_det, const float* x, const float* y, const uint32_t Nx, const uint32_t Ny, const uint32_t Nz, const float dx,
	const float dy, const float dz, const float bx, const float by, const float bz, const float bzb, const float maxxx, const float maxyy, const float zmax,
	const float NSlices, const uint32_t size_x, const uint16_t TotSinos, const bool verbose, const uint32_t loop_var_par, const char* k_path,
	const uint32_t* pseudos, const uint32_t det_per_ring, const uint32_t prows, const uint16_t* L, const uint8_t raw, const size_t size_z,
	const char* fileName, const uint32_t device, float kerroin, const size_t numel_x, const char* header_directory);

af::array im2col_3D(const af::array& A, const uint32_t blocksize1, const uint32_t blocksize2, const uint32_t blocksize3);

af::array sparseSum(const af::array& W, const uint32_t s);