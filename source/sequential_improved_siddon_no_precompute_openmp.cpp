/**************************************************************************
* Implements both the improved Siddon's algorithm and Orthogonal Siddon's 
* algorithm for OMEGA (Implementation 4).
* Determines which LORs intercept the FOV on-the-fly (slower). Improved
* Siddon can use up to 5 rays.
*
* Uses OpenMP for parallellization. If OpenMP is not available, the code
* is serial with no parallellization.
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
#include "projector_functions.h"
#ifdef _OPENMP
#include <omp.h>
#endif

// if 0, then determines whether the LOR intercepts the FOV (i.e. no precomputation phase performed)
constexpr int TYPE = 0;

// Whether to use the OpenMP code or not
constexpr bool OMP = true;

// Using non-OpenMP with either precomputation or without
constexpr bool PRECOMPUTE = false;

// Normalized distances below this are discarded in orthogonal ray tracer
constexpr auto THR = 0.01;

using namespace std;

void sequential_improved_siddon_no_precompute(const size_t loop_var_par, const uint32_t size_x, const double zmax, double* Summ, double* rhs, const double maxyy, const double maxxx,
	const vector<double>& xx_vec, const double dy, const vector<double>& yy_vec, const double* atten, const double* norm_coef, const double* randoms, const double* x, const double* y, const double* z_det,
	const uint32_t NSlices, const uint32_t Nx, const uint32_t Ny, const uint32_t Nz, const double dx, const double dz, const double bx, const double by, const double bz,
	const bool attenuation_correction, const bool normalization, const bool randoms_correction, const uint32_t* xy_index, const uint16_t* z_index, const uint32_t TotSinos,
	const double epps, const double* Sino, double* osem_apu, const uint16_t* L, const uint32_t* pseudos, const size_t pRows, const uint32_t det_per_ring,
	const bool raw, const double cr_pz, const bool no_norm, const uint16_t n_rays) {


	const uint32_t Nyx = Ny * Nx;

	const double bzb = bz + static_cast<double>(Nz) * dz;

	const double dc_z = cr_pz / 3.;

#pragma omp parallel for
	for (int32_t lo = 0; lo < loop_var_par; lo++) {

		const double local_sino = Sino[lo];
		if (no_norm && local_sino == 0.)
			continue;

		vector<int32_t> tempi_a(n_rays, 0);
		vector<int32_t> tempj_a(n_rays, 0);
		vector<int32_t> tempk_a(n_rays, 0);
		vector<int32_t> iu_a(n_rays, 0);
		vector<int32_t> ju_a(n_rays, 0);
		vector<int32_t> ku_a(n_rays, 0);
		vector<double> tx0_a(n_rays, 0.);
		vector<double> ty0_a(n_rays, 0.);
		vector<double> tz0_a(n_rays, 0.);
		vector<double> tc_a(n_rays, 0.);
		vector<double> txu_a(n_rays, 0.);
		vector<double> tyu_a(n_rays, 0.);
		vector<double> tzu_a(n_rays, 0.);
		vector<double> x_diff(n_rays, 0.);
		vector<double> y_diff(n_rays, 0.);
		vector<double> z_diff(n_rays, 0.);
		vector<double> LL(n_rays, 0.);
		vector<uint32_t> Np_n(n_rays, 0u);

		double temp = 0.;
		double ax = 0., jelppi = 0.;

		vector<bool> pass(n_rays, false);

		// Loop through the rays
		for (uint16_t lor = 0u; lor < n_rays; lor++) {

			Det detectors;

			// Raw list-mode data
			if (raw) {
				get_detector_coordinates_raw_N(det_per_ring, x, y, z_det, detectors, L, lo, pseudos, pRows, lor + 1u, dc_z);
			}
			// Sinogram data
			else {
				get_detector_coordinates_mr(x, y, z_det, size_x, detectors, xy_index, z_index, TotSinos, lo, lor + 1u, dc_z);
			}

			// Calculate the x, y and z distances of the detector pair
			y_diff[lor] = (detectors.yd - detectors.ys);
			x_diff[lor] = (detectors.xd - detectors.xs);
			z_diff[lor] = (detectors.zd - detectors.zs);
			// Skip certain cases (e.g. if the x- and y-coordinates are the same for both detectors, LOR between detector n and n)
			if ((y_diff[lor] == 0. && x_diff[lor] == 0. && z_diff[lor] == 0.) || (y_diff[lor] == 0. && x_diff[lor] == 0.)) {
				continue;
			}

			// Number of voxels the ray traverses
			uint32_t Np = 0u;


			if (fabs(z_diff[lor]) < 1e-8) {

				const uint32_t tempk = z_ring(zmax, detectors.zs, static_cast<double>(NSlices));

				if (fabs(y_diff[lor]) < 1e-8) {

					if (detectors.yd <= maxyy && detectors.yd >= by) {
						int32_t apu = 0;

						const double element = perpendicular_elements_multiray(Ny, detectors.yd, yy_vec, dx, tempk, Nx, Ny, atten, attenuation_correction,
							apu, 1u, jelppi);

						temp += element;
						tempk_a[lor] = apu;
						for (uint32_t k = 0; k < Nx; k++) {
							ax += (dx * osem_apu[tempk_a[lor] + k]);
						}
						pass[lor] = true;
					}
				}
				else if (fabs(x_diff[lor]) < 1e-8) {

					if (detectors.xd <= maxxx && detectors.xd >= bx) {
						int32_t apu = 0;
						const double element = perpendicular_elements_multiray(1u, detectors.xd, xx_vec, dy, tempk, Ny, Nx, atten, attenuation_correction,
							apu, Nx, jelppi);

						temp += element;
						tempk_a[lor] = apu;
						for (uint32_t k = 0; k < Ny; k++) {
							ax += (dy * osem_apu[tempk_a[lor] + k * Nx]);
						}
						pass[lor] = true;
					}
				}
				else {
					int32_t tempi = 0, tempj = 0, iu = 0, ju = 0;
					double txu = 0., tyu = 0., tc = 0., tx0 = 0., ty0 = 0.;

					const bool skip = siddon_pre_loop_2D(bx, by, x_diff[lor], y_diff[lor], maxxx, maxyy, dx, dy, Nx, Ny, tempi, tempj, txu, tyu, Np, TYPE,
						detectors.ys, detectors.xs, detectors.yd, detectors.xd, tc, iu, ju, tx0, ty0);

					// Skip if the LOR does not intersect with the voxel space
					if (skip || tempi < 0 || tempj < 0 || tempi >= static_cast<int32_t>(Nx) || tempj >= static_cast<int32_t>(Ny)) {
						continue;
					}

					// Save necessary variables at this ray
					LL[lor] = sqrt(x_diff[lor] * x_diff[lor] + y_diff[lor] * y_diff[lor]);

					tempi_a[lor] = tempi, tempj_a[lor] = tempj, tempk_a[lor] = tempk;
					tx0_a[lor] = tx0, ty0_a[lor] = ty0, tc_a[lor] = tc, tz0_a[lor] = 1e8;
					txu_a[lor] = txu, tyu_a[lor] = tyu, tzu_a[lor] = 1e8;
					iu_a[lor] = iu, ju_a[lor] = ju, ku_a[lor] = 0;
					uint32_t tempijk = tempk * Nyx + static_cast<uint32_t>(tempj) * Nx + static_cast<uint32_t>(tempi);

					// Compute the total distance traveled by this ray
					for (uint32_t ii = 0; ii < Np; ii++) {

						if (tx0 < ty0) {
							const double element = (tx0 - tc) * LL[lor];

							temp += element;
							ax += (element * osem_apu[tempijk]);
							if (attenuation_correction)
								jelppi += (element * -atten[tempijk]);

							tempi += iu;
							if (iu > 0)
								tempijk++;
							else
								tempijk--;

							tc = tx0;
							tx0 += txu;

						}
						else {

							const double element = (ty0 - tc) * LL[lor];

							temp += element;
							ax += (element * osem_apu[tempijk]);
							if (attenuation_correction)
								jelppi += (element * -atten[tempijk]);

							tempj += ju;
							if (ju > 0)
								tempijk += Nx;
							else
								tempijk -= Nx;

							tc = ty0;
							ty0 += tyu;
						}
						Np_n[lor]++;
						if (tempj < 0 || tempi < 0 || tempi >= static_cast<int32_t>(Nx) || tempj >= static_cast<int32_t>(Ny))
							break;
					}
					// This ray passed the voxel space
					pass[lor] = true;
				}
			}
			else {
				if (fabs(y_diff[lor]) < 1e-8) {
					if (detectors.yd <= maxyy && detectors.yd >= by) {

						int32_t tempi = 0, tempk = 0, tempj = 0, iu = 0, ku = 0;
						double txu = 0., tzu = 0., tc = 0., tx0 = 0., tz0 = 0.;

						const bool skip = siddon_pre_loop_2D(bx, bz, x_diff[lor], z_diff[lor], maxxx, bzb, dx, dz, Nx, Nz, tempi, tempk, txu, tzu, Np, TYPE,
							detectors.zs, detectors.xs, detectors.zd, detectors.xd, tc, iu, ku, tx0, tz0);

						if (skip || tempi < 0 || tempk < 0 || tempi >= static_cast<int32_t>(Nx) || tempk >= static_cast<int32_t>(Nz)) {
							continue;
						}
						LL[lor] = sqrt((x_diff[lor] * x_diff[lor] + z_diff[lor] * z_diff[lor]));
						double apu1;

						for (size_t ii = 0ULL; ii < static_cast<size_t>(Ny); ii++) {
							apu1 = (yy_vec[ii + 1ULL] - detectors.yd);
							if (apu1 > 0.) {
								tempj = static_cast<int32_t>(ii);
								break;
							}
						}

						tempi_a[lor] = tempi, tempj_a[lor] = tempj, tempk_a[lor] = tempk;
						tx0_a[lor] = tx0, ty0_a[lor] = 1e8, tc_a[lor] = tc, tz0_a[lor] = tz0;
						txu_a[lor] = txu, tyu_a[lor] = 1e8, tzu_a[lor] = tzu;
						iu_a[lor] = iu, ju_a[lor] = 0, ku_a[lor] = ku;
						uint32_t tempijk = Nyx * static_cast<uint32_t>(tempk) + static_cast<uint32_t>(tempj) * Nx + static_cast<uint32_t>(tempi);

						for (uint32_t ii = 0; ii < Np; ii++) {

							if (tx0 < tz0) {

								const double element = (tx0 - tc) * LL[lor];

								temp += element;
								ax += (element * osem_apu[tempijk]);
								if (attenuation_correction)
									jelppi += (element * -atten[tempijk]);


								if (iu > 0)
									tempijk++;
								else
									tempijk--;
								tempi += iu;
								tc = tx0;
								tx0 += txu;
							}
							else {

								const double element = (tz0 - tc) * LL[lor];

								temp += element;
								ax += (element * osem_apu[tempijk]);
								if (attenuation_correction)
									jelppi += (element * -atten[tempijk]);

								if (ku > 0)
									tempijk += Nyx;
								else
									tempijk -= Nyx;
								tempk += ku;
								tc = tz0;
								tz0 += tzu;
							}
							Np_n[lor]++;
							if (tempk < 0 || tempi < 0 || tempi >= static_cast<int32_t>(Nx) || tempk >= static_cast<int32_t>(Nz))
								break;

						}
						pass[lor] = true;

					}
				}
				else if (fabs(x_diff[lor]) < 1e-8) {
					if (detectors.xd <= maxxx && detectors.xd >= bx) {

						int32_t tempi = 0, tempk = 0, tempj = 0, ju = 0, ku = 0;
						double tyu = 0., tzu = 0., tc = 0., ty0 = 0., tz0 = 0.;
						const bool skip = siddon_pre_loop_2D(by, bz, y_diff[lor], z_diff[lor], maxyy, bzb, dy, dz, Ny, Nz, tempj, tempk, tyu, tzu, Np, TYPE,
							detectors.zs, detectors.ys, detectors.zd, detectors.yd, tc, ju, ku, ty0, tz0);

						if (skip || tempk < 0 || tempj < 0 || tempk >= static_cast<int32_t>(Nz) || tempj >= static_cast<int32_t>(Ny)) {
							continue;
						}
						LL[lor] = sqrt((y_diff[lor] * y_diff[lor] + z_diff[lor] * z_diff[lor]));
						double apu1;

						for (size_t ii = 0ULL; ii < static_cast<size_t>(Nx); ii++) {
							apu1 = (xx_vec[ii + 1ULL] - detectors.xd);
							if (apu1 > 0.) {
								tempi = static_cast<int32_t>(ii);
								break;
							}
						}

						tempi_a[lor] = tempi, tempj_a[lor] = tempj, tempk_a[lor] = tempk;
						tx0_a[lor] = 1e8, ty0_a[lor] = ty0, tc_a[lor] = tc, tz0_a[lor] = tz0;
						txu_a[lor] = 1e8, tyu_a[lor] = tyu, tzu_a[lor] = tzu;
						iu_a[lor] = 0, ju_a[lor] = ju, ku_a[lor] = ku;
						uint32_t tempijk = Nyx * static_cast<uint32_t>(tempk) + static_cast<uint32_t>(tempj) * Nx + static_cast<uint32_t>(tempi);

						for (uint32_t ii = 0; ii < Np; ii++) {

							if (ty0 < tz0) {

								const double element = (ty0 - tc) * LL[lor];

								temp += element;
								ax += (element * osem_apu[tempijk]);
								if (attenuation_correction)
									jelppi += (element * -atten[tempijk]);


								if (ju > 0)
									tempijk += Nx;
								else
									tempijk -= Nx;
								tc = ty0;
								ty0 += tyu;
								tempj += ju;
							}
							else {

								const double element = (tz0 - tc) * LL[lor];

								temp += element;
								ax += (element * osem_apu[tempijk]);
								if (attenuation_correction)
									jelppi += (element * -atten[tempijk]);

								if (ku > 0)
									tempijk += Nyx;
								else
									tempijk -= Nyx;
								tc = tz0;
								tz0 += tzu;
								tempk += ku;
							}
							Np_n[lor]++;
							if (tempj < 0 || tempk < 0 || tempk >= static_cast<int32_t>(Nz) || tempj >= static_cast<int32_t>(Ny))
								break;

						}
						pass[lor] = true;
					}
				}
				else {

					int32_t tempi = 0, tempj = 0, tempk = 0, iu = 0, ju = 0, ku = 0;
					double txu = 0., tyu = 0., tzu = 0., tc = 0., tx0 = 0., ty0 = 0., tz0 = 0.;
					const bool skip = siddon_pre_loop_3D(bx, by, bz, x_diff[lor], y_diff[lor], z_diff[lor], maxxx, maxyy, bzb, dx, dy, dz, Nx, Ny, Nz,
						tempi, tempj, tempk, tyu, txu, tzu, Np, TYPE, detectors, tc, iu, ju, ku, tx0, ty0, tz0);

					if (skip || tempi < 0 || tempj < 0 || tempk < 0 || tempi >= static_cast<int32_t>(Nx) || tempj >= static_cast<int32_t>(Ny)
						|| tempk >= static_cast<int32_t>(Nz)) {
						continue;
					}
					LL[lor] = sqrt(x_diff[lor] * x_diff[lor] + z_diff[lor] * z_diff[lor] + y_diff[lor] * y_diff[lor]);

					tempi_a[lor] = tempi, tempj_a[lor] = tempj, tempk_a[lor] = tempk;
					tx0_a[lor] = tx0, ty0_a[lor] = ty0, tc_a[lor] = tc, tz0_a[lor] = tz0;
					txu_a[lor] = txu, tyu_a[lor] = tyu, tzu_a[lor] = tzu;
					iu_a[lor] = iu, ju_a[lor] = ju, ku_a[lor] = ku;
					uint32_t tempijk = Nyx * static_cast<uint32_t>(tempk) + static_cast<uint32_t>(tempj) * Nx + static_cast<uint32_t>(tempi);

					for (uint32_t ii = 0; ii < Np; ii++) {
						if (tz0 < ty0 && tz0 < tx0) {

							const double element = (tz0 - tc) * LL[lor];

							temp += element;
							ax += (element * osem_apu[tempijk]);
							if (attenuation_correction)
								jelppi += (element * -atten[tempijk]);

							if (ku > 0)
								tempijk += Nyx;
							else
								tempijk -= Nyx;
							tc = tz0;
							tz0 += tzu;
							tempk += ku;
						}
						else if (ty0 < tx0) {
							const double element = (ty0 - tc) * LL[lor];

							temp += element;
							ax += (element * osem_apu[tempijk]);
							if (attenuation_correction)
								jelppi += (element * -atten[tempijk]);

							if (ju > 0)
								tempijk += Nx;
							else
								tempijk -= Nx;
							tc = ty0;
							ty0 += tyu;
							tempj += ju;
						}
						else {
							const double element = (tx0 - tc) * LL[lor];

							temp += element;
							ax += (element * osem_apu[tempijk]);
							if (attenuation_correction)
								jelppi += (element * -atten[tempijk]);

							if (iu > 0)
								tempijk++;
							else
								tempijk--;
							tc = tx0;
							tx0 += txu;
							tempi += iu;
						}
						Np_n[lor]++;
						if (tempj < 0 || tempi < 0 || tempk < 0 || tempi >= static_cast<int32_t>(Nx) || tempj >= static_cast<int32_t>(Ny)
							|| tempk >= static_cast<int32_t>(Nz))
							break;
					}
					pass[lor] = true;
				}
			}
		}

		bool alku = true;
		double yax = 0.;

		// Compute the probabilities for the current LOR
		// Sum all the rays together
		for (uint16_T lor = 0u; lor < n_rays; lor++) {

			if (pass[lor]) {

				if (alku) {
					temp = 1. / temp;
					if (attenuation_correction) {
						double n_r_summa = 0.;
						for (uint16_T ln_r = 0u; ln_r < n_rays; ln_r++)
							n_r_summa += static_cast<double>(pass[ln_r]);
						temp *= exp(jelppi / n_r_summa);
					}
					if (normalization)
						temp *= norm_coef[lo];
					if (local_sino != 0.) {
						if (ax == 0.) {
							ax = epps;
						}
						else {
							ax *= temp;
						}
						if (randoms_correction)
							ax += randoms[lo];
						yax = local_sino / ax;
					}
					alku = false;
				}

				if (fabs(z_diff[lor]) < 1e-8) {

					if (fabs(y_diff[lor]) < 1e-8) {

						if (local_sino != 0.) {
							for (uint32_t k = 0; k < Nx; k++) {
#pragma omp atomic
								rhs[tempk_a[lor] + k] += (dx * temp * yax);
								if (no_norm == 0) {
#pragma omp atomic
									Summ[tempk_a[lor] + k] += (dx * temp);
								}
							}
						}
						else {
							for (uint32_t k = 0; k < Nx; k++) {
								if (no_norm == 0) {
#pragma omp atomic
									Summ[tempk_a[lor] + k] += (dx * temp);
								}
							}
						}
					}
					else if (fabs(x_diff[lor]) < 1e-8) {
						if (local_sino != 0.) {
							for (uint32_t k = 0; k < Ny; k++) {
#pragma omp atomic
								rhs[tempk_a[lor] + k * Nx] += (dy * temp * yax);
								if (no_norm == 0) {
#pragma omp atomic
									Summ[tempk_a[lor] + k * Nx] += (dy * temp);
								}
							}
						}
						else {
							for (uint32_t k = 0; k < Ny; k++) {
								if (no_norm == 0) {
#pragma omp atomic
									Summ[tempk_a[lor] + k * Nx] += (dy * temp);
								}
							}
						}
					}
					else {
						double tx0 = tx0_a[lor];
						double ty0 = ty0_a[lor];
						const double txu = txu_a[lor];
						const double tyu = tyu_a[lor];
						const int32_t tempi = tempi_a[lor];
						const int32_t tempj = tempj_a[lor];
						const int32_t tempk = tempk_a[lor];
						const int32_t iu = iu_a[lor];
						const int32_t ju = ju_a[lor];
						double tc = tc_a[lor];
						int32_T tempijk = tempk * Nyx + static_cast<uint32_t>(tempj) * Nx + static_cast<uint32_t>(tempi);

						if (local_sino != 0.) {
							for (uint32_t ii = 0; ii < Np_n[lor]; ii++) {

								if (tx0 < ty0) {
									const double element = (tx0 - tc) * LL[lor] * temp;
#pragma omp atomic
									rhs[tempijk] += (element * yax);
									if (no_norm == 0) {
#pragma omp atomic
										Summ[tempijk] += element;
									}

									if (iu > 0)
										tempijk++;
									else
										tempijk--;
									tc = tx0;
									tx0 += txu;


								}
								else {

									const double element = (ty0 - tc) * LL[lor] * temp;

#pragma omp atomic
									rhs[tempijk] += (element * yax);
									if (no_norm == 0) {
#pragma omp atomic
										Summ[tempijk] += element;
									}

									if (ju > 0)
										tempijk += Nx;
									else
										tempijk -= Nx;
									tc = ty0;
									ty0 += tyu;
								}
							}
						}
						else {
							for (uint32_t ii = 0; ii < Np_n[lor]; ii++) {

								if (tx0 < ty0) {
									const double element = (tx0 - tc) * LL[lor] * temp;

									if (no_norm == 0) {
#pragma omp atomic
										Summ[tempijk] += element;
									}

									if (iu > 0)
										tempijk++;
									else
										tempijk--;
									tc = tx0;
									tx0 += txu;


								}
								else {

									const double element = (ty0 - tc) * LL[lor] * temp;

									if (no_norm == 0) {
#pragma omp atomic
										Summ[tempijk] += element;
									}

									if (ju > 0)
										tempijk += Nx;
									else
										tempijk -= Nx;
									tc = ty0;
									ty0 += tyu;
								}
							}
						}
					}
				}
				else {
					double tx0 = tx0_a[lor];
					double ty0 = ty0_a[lor];
					double tz0 = tz0_a[lor];
					const double txu = txu_a[lor];
					const double tyu = tyu_a[lor];
					const double tzu = tzu_a[lor];
					const int32_t tempi = tempi_a[lor];
					const int32_t tempj = tempj_a[lor];
					const int32_t tempk = tempk_a[lor];
					const int32_t iu = iu_a[lor];
					const int32_t ju = ju_a[lor];
					const int32_t ku = ku_a[lor];
					double tc = tc_a[lor];
					uint32_t tempijk = Nyx * static_cast<uint32_t>(tempk) + static_cast<uint32_t>(tempj) * Nx + static_cast<uint32_t>(tempi);

					if (local_sino != 0.) {
						for (uint32_t ii = 0; ii < Np_n[lor]; ii++) {
							if (tz0 < ty0 && tz0 < tx0) {

								const double element = (tz0 - tc) * LL[lor] * temp;

#pragma omp atomic
								rhs[tempijk] += (element * yax);
								if (no_norm == 0) {
#pragma omp atomic
									Summ[tempijk] += element;
								}

								if (ku > 0)
									tempijk += Nyx;
								else
									tempijk -= Nyx;
								tc = tz0;
								tz0 += tzu;
							}
							else if (ty0 < tx0 && ty0 <= tz0) {
								const double element = (ty0 - tc) * LL[lor] * temp;

#pragma omp atomic
								rhs[tempijk] += (element * yax);
								if (no_norm == 0) {
#pragma omp atomic
									Summ[tempijk] += element;
								}

								if (ju > 0)
									tempijk += Nx;
								else
									tempijk -= Nx;
								tc = ty0;
								ty0 += tyu;
							}
							else if (tx0 <= ty0 && tx0 <= tz0) {
								const double element = (tx0 - tc) * LL[lor] * temp;

#pragma omp atomic
								rhs[tempijk] += (element * yax);
								if (no_norm == 0) {
#pragma omp atomic
									Summ[tempijk] += element;
								}

								if (iu > 0)
									tempijk++;
								else
									tempijk--;
								tc = tx0;
								tx0 += txu;
							}

						}

					}
					else {
						for (uint32_t ii = 0; ii < Np_n[lor]; ii++) {
							if (tz0 < ty0 && tz0 < tx0) {

								const double element = (tz0 - tc) * LL[lor] * temp;

								if (no_norm == 0) {
#pragma omp atomic
									Summ[tempijk] += element;
								}

								if (ku > 0)
									tempijk += Nyx;
								else
									tempijk -= Nyx;
								tc = tz0;
								tz0 += tzu;
							}
							else if (ty0 < tx0 && ty0 <= tz0) {
								const double element = (ty0 - tc) * LL[lor] * temp;

								if (no_norm == 0) {
#pragma omp atomic
									Summ[tempijk] += element;
								}

								if (ju > 0)
									tempijk += Nx;
								else
									tempijk -= Nx;
								tc = ty0;
								ty0 += tyu;
							}
							else if (tx0 <= ty0 && tx0 <= tz0) {
								const double element = (tx0 - tc) * LL[lor] * temp;

								if (no_norm == 0) {
#pragma omp atomic
									Summ[tempijk] += element;
								}

								if (iu > 0)
									tempijk++;
								else
									tempijk--;
								tc = tx0;
								tx0 += txu;
							}
						}
					}
				}
			}
		}
	}
}

void sequential_orth_siddon_no_precomp(const size_t loop_var_par, const uint32_t size_x, const double zmax, double* Summ, double* rhs, const double maxyy, 
	const double maxxx,	const vector<double>& xx_vec, const double dy, const vector<double>& yy_vec, const double* atten, const double* norm_coef, 
	const double* randoms, const double* x, const double* y, const double* z_det, const uint32_t NSlices, const uint32_t Nx, const uint32_t Ny, 
	const uint32_t Nz, const double dx, const double dz, const double bx, const double by, const double bz, const bool attenuation_correction, 
	const bool normalization, const bool randoms_correction, const uint32_t* xy_index, const uint16_t* z_index, const uint32_t TotSinos,
	const double epps, const double* Sino, double* osem_apu, const uint16_t* L, const uint32_t* pseudos, const uint32_t pRows, const uint32_t det_per_ring,
	const bool raw, const double crystal_size_xy, const double* x_center, const double* y_center, const double* z_center, const double crystal_size_z, 
	const bool no_norm, const int32_t dec_v) {

	const uint32_t Nyx = Ny * Nx;

	const double bzb = bz + static_cast<double>(Nz) * dz;

	const int32_T dec = static_cast<int32_T>(ceil(crystal_size_z / sqrt(dz * dz * 2.))) * dec_v;

	size_t idx = 0ULL;
	vector<double> elements;
	vector<uint32_t> v_indices;

#pragma omp parallel for
	for (uint32_t lo = 0u; lo < loop_var_par; lo++) {


		const double local_sino = Sino[lo];
		if (no_norm && local_sino == 0.)
			continue;

		Det detectors;
		double kerroin, length_;

		// Raw list-mode data
		if (raw) {
			get_detector_coordinates_raw(det_per_ring, x, y, z_det, detectors, L, lo, pseudos, pRows);
		}
		// Sinogram data
		else {
			get_detector_coordinates(x, y, z_det, size_x, detectors, xy_index, z_index, TotSinos, lo);
		}

		// Calculate the x, y and z distances of the detector pair
		const double x_diff = (detectors.xd - detectors.xs);
		const double y_diff = (detectors.yd - detectors.ys);
		const double z_diff = (detectors.zd - detectors.zs);
		if ((y_diff == 0. && x_diff == 0. && z_diff == 0.) || (y_diff == 0. && x_diff == 0.))
			continue;

		double ax = 0., jelppi = 0., LL;
		uint32_t Np = 0u;
		uint32_t Np_n = 0u;
		uint8_t xyz = 0u;

		if (crystal_size_z == 0.) {
			kerroin = detectors.xd * detectors.ys - detectors.yd * detectors.xs;
			length_ = sqrt(y_diff * y_diff + x_diff * x_diff) * crystal_size_xy;
		}
		else {
			kerroin = norm(x_diff, y_diff, z_diff) * crystal_size_z;
		}

		if (fabs(z_diff) < 1e-8) {

			const uint32_t tempk = z_ring(zmax, detectors.zs, static_cast<double>(NSlices));

			if (fabs(y_diff) < 1e-8) {

				if (detectors.yd <= maxyy && detectors.yd >= by) {
					if (crystal_size_z == 0.) {
						double temp = 0.;
						orth_distance_denominator_perpendicular_mfree(-x_diff, y_center, kerroin, length_, temp, attenuation_correction, ax,
							by, detectors.yd, dy, Ny, Nx, tempk, atten, local_sino, Ny, 1u, osem_apu);
						if (local_sino != 0.) {
							nominator_mfree(ax, local_sino, epps, temp, randoms_correction, randoms, lo);
							orth_distance_rhs_perpendicular_mfree(-x_diff, y_center, kerroin, length_, temp, ax, by, detectors.yd, dy, Ny, Nx, tempk, Ny, 
								1u, no_norm, rhs, Summ);
						}
						else {
							orth_distance_summ_perpendicular_mfree(-x_diff, y_center, kerroin, length_, temp, ax, by, detectors.yd, dy, Ny, Nx, tempk, Ny, 
								1u, Summ);
						}
					}
					else {
						double temppi = detectors.xs;
						detectors.xs = detectors.ys;
						detectors.ys = temppi;
						double temp = 0.;
						orth_distance_denominator_perpendicular_mfree_3D(y_center, x_center[0], z_center, temp, attenuation_correction, ax,
							by, detectors.yd, dy, Ny, Nx, tempk, atten, local_sino, Ny, 1u, osem_apu, detectors, y_diff, x_diff, z_diff, kerroin, Nyx, Nz);
						if (local_sino != 0.) {
							nominator_mfree(ax, local_sino, epps, temp, randoms_correction, randoms, lo);
							orth_distance_rhs_perpendicular_mfree_3D(y_center, x_center[0], z_center, temp, ax, by, detectors.yd, dy, Ny, Nx, tempk, Ny, 
								1u, no_norm, rhs, Summ, detectors, y_diff, x_diff, z_diff, kerroin, Nyx, Nz);
						}
						else {
							orth_distance_summ_perpendicular_mfree_3D(y_center, x_center[0], z_center, temp, ax, by, detectors.yd, dy, Ny, Nx, tempk, Ny, 1u, 
								Summ, detectors, y_diff, x_diff, z_diff, kerroin, Nyx, Nz);
						}
					}
				}
			}
			else if (fabs(x_diff) < 1e-8) {

				if (detectors.xd <= maxxx && detectors.xd >= bx) {
					if (crystal_size_z == 0.) {
						double temp = 0.;
						orth_distance_denominator_perpendicular_mfree(y_diff, x_center, kerroin, length_, temp, attenuation_correction, ax,
							bx, detectors.xd, dx, Nx, Ny, tempk, atten, local_sino, 1u, Nx, osem_apu);
						if (local_sino != 0.) {
							nominator_mfree(ax, local_sino, epps, temp, randoms_correction, randoms, lo);
							orth_distance_rhs_perpendicular_mfree(y_diff, x_center, kerroin, length_, temp, ax, bx, detectors.xd, dx, Nx, Ny, tempk, 
								1u, Nx, no_norm, rhs, Summ);
						}
						else {
							orth_distance_summ_perpendicular_mfree(y_diff, x_center, kerroin, length_, temp, ax, bx, detectors.xd, dx, Nx, Ny, tempk, 
								1u, Nx, Summ);
						}
					}
					else {
						double temp = 0.;
						orth_distance_denominator_perpendicular_mfree_3D(x_center, y_center[0], z_center, temp, attenuation_correction, ax,
							bx, detectors.xd, dx, Nx, Ny, tempk, atten, local_sino, 1u, Nx, osem_apu, detectors, x_diff, y_diff, z_diff, kerroin, Nyx, Nz);
						if (local_sino != 0.) {
							nominator_mfree(ax, local_sino, epps, temp, randoms_correction, randoms, lo);
							orth_distance_rhs_perpendicular_mfree_3D(x_center, y_center[0], z_center, temp, ax, bx, detectors.xd, dx, Nx, Ny, tempk, 
								1u, Nx, no_norm, rhs, Summ, detectors, x_diff, y_diff, z_diff, kerroin, Nyx, Nz);
						}
						else {
							orth_distance_summ_perpendicular_mfree_3D(x_center, y_center[0], z_center, temp, ax, bx, detectors.xd, dx, Nx, Ny, tempk, 1u, Nx, 
								Summ, detectors, x_diff, y_diff, z_diff, kerroin, Nyx, Nz);
						}
					}
				}
			}
			else {
				int32_t tempi = 0, tempj = 0, iu = 0, ju = 0;
				double txu = 0., tyu = 0., tc = 0., tx0 = 0., ty0 = 0.;

				const bool skip = siddon_pre_loop_2D(bx, by, x_diff, y_diff, maxxx, maxyy, dx, dy, Nx, Ny, tempi, tempj, txu, tyu, Np, TYPE,
					detectors.ys, detectors.xs, detectors.yd, detectors.xd, tc, iu, ju, tx0, ty0);

				if (skip)
					continue;

				if (attenuation_correction)
					LL = sqrt(x_diff * x_diff + y_diff * y_diff);
				double temp = 0.;
				int32_t tempi_a = tempi, tempj_a = tempj;
				double tx0_a = tx0, ty0_a = ty0;
				uint32_t tempijk;
				if (crystal_size_z == 0.)
					tempijk = Nyx * tempk + static_cast<uint32_t>(tempj) * Nx;
				else
					tempijk = static_cast<uint32_t>(tempj) * Nx;

				for (uint32_t ii = 0u; ii < Np; ii++) {


					if (tx0 < ty0) {
						if (attenuation_correction)
							compute_attenuation(tc, jelppi, LL, tx0, tempi, tempj, tempk, Nx, Nyx, atten);
						if (ii == Np - 1u) {
							if (crystal_size_z == 0.) {
								orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
									tempj, local_sino, ax, osem_apu, no_norm, false, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							else {
								orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
									tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, false, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
						}
						else {
							tempi += iu;
							tx0 += txu;
						}
						xyz = 1u;
					}
					else {
						if (attenuation_correction)
							compute_attenuation(tc, jelppi, LL, ty0, tempi, tempj, tempk, Nx, Nyx, atten);
						if (crystal_size_z == 0.) {
							orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
								tempj, local_sino, ax, osem_apu, no_norm, false, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
						}
						else {
							orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
								tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, false, OMP, 
								PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
						}

						if (ju > 0) {
							tempijk += Nx;
						}
						else {
							tempijk -= Nx;
						}
						tempj += ju;
						ty0 += tyu;
						xyz = 2u;
					}
					Np_n++;
					if (tempj < 0 || tempi < 0 || tempi >= static_cast<int32_t>(Nx) || tempj >= static_cast<int32_t>(Ny)) {
						if (xyz == 1u && ii != Np - 1u) {
							tempi -= iu;
							if (crystal_size_z == 0.) {
								orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
									tempj, local_sino, ax, osem_apu, no_norm, false, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							else {
								orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
									tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, false, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
						}
						break;
					}
				}

				temp = 1. / temp;
				tx0 = tx0_a;
				ty0 = ty0_a;
				tempi = tempi_a;
				tempj = tempj_a;
				if (crystal_size_z == 0.)
					tempijk = Nyx * tempk + static_cast<uint32_t>(tempj) * Nx;
				else
					tempijk = static_cast<uint32_t>(tempj)* Nx;
				if (attenuation_correction)
					temp *= exp(jelppi);
				if (normalization)
					temp *= norm_coef[lo];

				if (local_sino != 0.) {
					nominator_mfree(ax, local_sino, epps, temp, randoms_correction, randoms, lo);
					for (uint32_t ii = 0u; ii < Np_n; ii++) {
						if (tx0 < ty0) {
							if (ii == Np_n - 1u) {
								if (crystal_size_z == 0.) {
									orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
										tempj, local_sino, ax, osem_apu, no_norm, true, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
								else {
									orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
										tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, true, false, OMP, 
										PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
							}
							else {
								tempi += iu;
								tx0 += txu;
							}
						}
						else {
							if (crystal_size_z == 0.) {
								orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
									tempj, local_sino, ax, osem_apu, no_norm, true, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							else {
								orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
									tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, true, false, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							if (ju > 0) {
								tempijk += Nx;
							}
							else {
								tempijk -= Nx;
							}
							tempj += ju;
							ty0 += tyu;
						}
					}
				}
				else {
					for (uint32_t ii = 0u; ii < Np_n; ii++) {
						if (tx0 < ty0) {
							if (ii == Np_n - 1u) {
								if (crystal_size_z == 0.) {
									orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
										tempj, local_sino, ax, osem_apu, no_norm, false, true, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
								else {
									orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
										tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, true, OMP, 
										PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
							}
							else {
								tempi += iu;
								tx0 += txu;
							}
						}
						else {
							if (crystal_size_z == 0.) {
								orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
									tempj, local_sino, ax, osem_apu, no_norm, false, true, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							else {
								orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
									tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, true, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							if (ju > 0) {
								tempijk += Nx;
							}
							else {
								tempijk -= Nx;
							}
							tempj += ju;
							ty0 += tyu;
						}
					}
				}
			}
		}
		else {

			if (fabs(y_diff) < 1e-8) {
				if (detectors.yd <= maxyy && detectors.yd >= by) {

					int32_t tempi = 0, tempk = 0, tempj = 0, iu = 0, ku = 0;
					double txu = 0., tzu = 0., tc = 0., tx0 = 0., tz0 = 0.;

					const bool skip = siddon_pre_loop_2D(bx, bz, x_diff, z_diff, maxxx, bzb, dx, dz, Nx, Nz, tempi, tempk, txu, tzu, Np, TYPE,
						detectors.zs, detectors.xs, detectors.zd, detectors.xd, tc, iu, ku, tx0, tz0);

					if (skip)
						continue;

					double apu1;
					if (attenuation_correction)
						LL = sqrt(x_diff * x_diff + z_diff * z_diff);

					for (size_t ii = 0ULL; ii < static_cast<size_t>(Ny); ii++) {
						apu1 = (yy_vec[ii + 1ULL] - detectors.yd);
						if (apu1 > 0.) {
							tempj = static_cast<int32_t>(ii);
							break;
						}
					}

					double temp = 0.;
					int32_t tempi_a = tempi, tempk_a = tempk;
					double tx0_a = tx0, tz0_a = tz0;
					uint32_t tempijk;
					if (crystal_size_z == 0.)
						tempijk = Nyx * static_cast<uint32_t>(tempk) + static_cast<uint32_t>(tempi);
					else {
						tempijk = static_cast<uint32_t>(tempi);
						const double temp_x = detectors.xs;
						detectors.xs = detectors.ys;
						detectors.ys = temp_x;
					}

					for (uint32_t ii = 0u; ii < Np; ii++) {
						if (tx0 < tz0) {
							if (attenuation_correction)
								compute_attenuation(tc, jelppi, LL, tx0, tempi, tempj, tempk, Nx, Nyx, atten);
							if (crystal_size_z == 0.) {
								orth_distance_full(tempj, Ny, -x_diff, -y_diff, x_center[tempi], y_center, kerroin, length_, temp, tempijk, Nx,
									tempi, local_sino, ax, osem_apu, no_norm, false, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							else {
								orth_distance_3D_full(tempj, Ny, Nz, x_diff, y_diff, z_diff, x_center[tempi], y_center, z_center, temp, tempijk, Nx,
									tempi, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, false, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							if (iu > 0) {
								tempijk++;
							}
							else {
								tempijk--;
							}
							tempi += iu;
							tx0 += txu;
							xyz = 1u;
						}
						else {
							if (attenuation_correction)
								compute_attenuation(tc, jelppi, LL, tz0, tempi, tempj, tempk, Nx, Nyx, atten);
							if (crystal_size_z == 0.) {
								orth_distance_full(tempj, Ny, -x_diff, -y_diff, x_center[tempi], y_center, kerroin, length_, temp, tempijk, Nx,
									tempi, local_sino, ax, osem_apu, no_norm, false, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								if (ku > 0)
									tempijk += Nyx;
								else
									tempijk -= Nyx;
							}
							else if (ii == Np - 1u) {
								orth_distance_3D_full(tempj, Ny, Nz, x_diff, y_diff, z_diff, x_center[tempi], y_center, z_center, temp, tempijk, Nx,
									tempi, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, false, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							tempk += ku;
							tz0 += tzu;
							xyz = 3u;
						}
						Np_n++;
						if (tempk < 0 || tempi < 0 || tempi >= static_cast<int32_t>(Nx) || tempk >= static_cast<int32_t>(Nz)) {
							if (crystal_size_z != 0.f && xyz == 3u && ii != Np - 1u) {
								tempk -= ku;
								orth_distance_3D_full(tempj, Ny, Nz, x_diff, y_diff, z_diff, x_center[tempi], y_center, z_center, temp, tempijk, Nx,
									tempi, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, false, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							break;
						}
					}
					temp = 1. / temp;
					tx0 = tx0_a;
					tz0 = tz0_a;
					tempi = tempi_a;
					tempk = tempk_a;
					if (crystal_size_z == 0.)
						tempijk = Nyx * static_cast<uint32_t>(tempk) + static_cast<uint32_t>(tempi);
					else
						tempijk = static_cast<uint32_t>(tempi);
					if (attenuation_correction)
						temp *= exp(jelppi);
					if (normalization)
						temp *= norm_coef[lo];
					if (local_sino != 0.) {
						nominator_mfree(ax, local_sino, epps, temp, randoms_correction, randoms, lo);
						for (uint32_t ii = 0u; ii < Np_n; ii++) {

							if (tx0 < tz0) {
								if (crystal_size_z == 0.) {
									orth_distance_full(tempj, Ny, -x_diff, -y_diff, x_center[tempi], y_center, kerroin, length_, temp, tempijk, Nx,
										tempi, local_sino, ax, osem_apu, no_norm, true, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
								else {
									orth_distance_3D_full(tempj, Ny, Nz, x_diff, y_diff, z_diff, x_center[tempi], y_center, z_center, temp, tempijk, Nx,
										tempi, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, true, false, OMP, 
										PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}

								if (iu > 0) {
									tempijk++;
								}
								else {
									tempijk--;
								}
								tempi += iu;
								tx0 += txu;
							}
							else {
								if (crystal_size_z == 0.) {
									orth_distance_full(tempj, Ny, -x_diff, -y_diff, x_center[tempi], y_center, kerroin, length_, temp, tempijk, Nx,
										tempi, local_sino, ax, osem_apu, no_norm, true, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);

									if (ku > 0)
										tempijk += Nyx;
									else
										tempijk -= Nyx;
								}
								else if (ii == Np_n - 1u) {
									orth_distance_3D_full(tempj, Ny, Nz, x_diff, y_diff, z_diff, x_center[tempi], y_center, z_center, temp, tempijk, Nx,
										tempi, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, true, false, OMP, 
										PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
								tempk += ku;
								tz0 += tzu;
							}
						}
					}
					else {
						for (uint32_t ii = 0u; ii < Np_n; ii++) {

							if (tx0 < tz0) {
								if (crystal_size_z == 0.) {
									orth_distance_full(tempj, Ny, -x_diff, -y_diff, x_center[tempi], y_center, kerroin, length_, temp, tempijk, Nx,
										tempi, local_sino, ax, osem_apu, no_norm, false, true, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
								else {
									orth_distance_3D_full(tempj, Ny, Nz, x_diff, y_diff, z_diff, x_center[tempi], y_center, z_center, temp, tempijk, Nx,
										tempi, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, true, OMP, 
										PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}

								if (iu > 0) {
									tempijk++;
								}
								else {
									tempijk--;
								}
								tempi += iu;
								tx0 += txu;
							}
							else {
								if (crystal_size_z == 0.) {
									orth_distance_full(tempj, Ny, -x_diff, -y_diff, x_center[tempi], y_center, kerroin, length_, temp, tempijk, Nx,
										tempi, local_sino, ax, osem_apu, no_norm, false, true, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);

									if (ku > 0)
										tempijk += Nyx;
									else
										tempijk -= Nyx;
								}
								else if (ii == Np_n - 1u) {
									orth_distance_3D_full(tempj, Ny, Nz, x_diff, y_diff, z_diff, x_center[tempi], y_center, z_center, temp, tempijk, Nx,
										tempi, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, true, OMP, 
										PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
								tempk += ku;
								tz0 += tzu;
							}
						}
					}
				}
			}
			else if (fabs(x_diff) < 1e-8) {
				if (detectors.xd <= maxxx && detectors.xd >= bx) {

					int32_t tempi = 0, tempk = 0, tempj = 0, ju = 0, ku = 0;
					double tyu = 0., tzu = 0., tc = 0., ty0 = 0., tz0 = 0.;
					const bool skip = siddon_pre_loop_2D(by, bz, y_diff, z_diff, maxyy, bzb, dy, dz, Ny, Nz, tempj, tempk, tyu, tzu, Np, TYPE,
						detectors.zs, detectors.ys, detectors.zd, detectors.yd, tc, ju, ku, ty0, tz0);

					if (skip)
						continue;

					double apu1;
					double temp = 0.;

					if (attenuation_correction)
						LL = sqrt(z_diff * z_diff + y_diff * y_diff);
					for (size_t ii = 0ULL; ii < static_cast<size_t>(Nx); ii++) {
						apu1 = (xx_vec[ii + 1ULL] - detectors.xd);
						if (apu1 > 0.) {
							tempi = static_cast<int32_t>(ii);
							break;
						}
					}

					int32_t tempj_a = tempj, tempk_a = tempk;
					double ty0_a = ty0, tz0_a = tz0;
					uint32_t tempijk;
					if (crystal_size_z == 0.)
						tempijk = Nyx * static_cast<uint32_t>(tempk) + static_cast<uint32_t>(tempj) * Nx;
					else
						tempijk = static_cast<uint32_t>(tempj) * Nx;

					for (uint32_t ii = 0u; ii < Np; ii++) {

						if (ty0 < tz0) {
							if (attenuation_correction)
								compute_attenuation(tc, jelppi, LL, ty0, tempi, tempj, tempk, Nx, Nyx, atten);
							if (crystal_size_z == 0.) {
								orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
									tempj, local_sino, ax, osem_apu, no_norm, false, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							else {
								orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
									tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  ju, no_norm, false, false, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}

							if (ju > 0) {
								tempijk += Nx;
							}
							else {
								tempijk -= Nx;
							}
							tempj += ju;
							ty0 += tyu;
							xyz = 2u;
						}
						else {
							if (attenuation_correction)
								compute_attenuation(tc, jelppi, LL, tz0, tempi, tempj, tempk, Nx, Nyx, atten);
							if (crystal_size_z == 0.) {
								orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
									tempj, local_sino, ax, osem_apu, no_norm, false, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);

								if (ku > 0)
									tempijk += Nyx;
								else
									tempijk -= Nyx;
							}
							else if (ii == Np - 1u) {
								orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
									tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  ju, no_norm, false, false, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							tempk += ku;
							tz0 += tzu;
							xyz = 3u;
						}
						Np_n++;
						if (tempj < 0 || tempk < 0 || tempk >= static_cast<int32_t>(Nz) || tempj >= static_cast<int32_t>(Ny)) {
							if (xyz == 3u && crystal_size_z != 0.f && ii != Np - 1u) {
								tempk -= ku;
								orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
									tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  ju, no_norm, false, false, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							break;
						}
					}

					temp = 1. / temp;
					ty0 = ty0_a;
					tz0 = tz0_a;
					tempj = tempj_a;
					tempk = tempk_a;
					if (crystal_size_z == 0.)
						tempijk = Nyx * static_cast<uint32_t>(tempk) + static_cast<uint32_t>(tempj) * Nx;
					else
						tempijk = static_cast<uint32_t>(tempj) * Nx;
					if (attenuation_correction)
						temp *= exp(jelppi);
					if (normalization)
						temp *= norm_coef[lo];


					if (local_sino != 0.) {
						nominator_mfree(ax, local_sino, epps, temp, randoms_correction, randoms, lo);
						for (uint32_t ii = 0u; ii < Np_n; ii++) {

							if (ty0 < tz0) {
								if (crystal_size_z == 0.) {
									orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
										tempj, local_sino, ax, osem_apu, no_norm, true, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
								else {
									orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
										tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  ju, no_norm, true, false, OMP, 
										PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}

								if (ju > 0) {
									tempijk += Nx;
								}
								else {
									tempijk -= Nx;
								}
								tempj += ju;
								ty0 += tyu;
							}
							else {
								if (crystal_size_z == 0.) {
									orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
										tempj, local_sino, ax, osem_apu, no_norm, true, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);

									if (ku > 0)
										tempijk += Nyx;
									else
										tempijk -= Nyx;
								}
								else if (ii == Np_n - 1u) {
									orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
										tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  ju, no_norm, true, false, OMP, 
										PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
								tempk += ku;
								tz0 += tzu;
							}
						}
					}
					else {
						for (uint32_t ii = 0u; ii < Np_n; ii++) {

							if (ty0 < tz0) {
								if (crystal_size_z == 0.) {
									orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
										tempj, local_sino, ax, osem_apu, no_norm, false, true, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
								else {
									orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
										tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  ju, no_norm, true, false, OMP, 
										PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}

								if (ju > 0) {
									tempijk += Nx;
								}
								else {
									tempijk -= Nx;
								}
								tempj += ju;
								ty0 += tyu;
							}
							else {
								if (crystal_size_z == 0.) {
									orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
										tempj, local_sino, ax, osem_apu, no_norm, false, true, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);

									if (ku > 0)
										tempijk += Nyx;
									else
										tempijk -= Nyx;
								}
								else if (ii == Np_n - 1u) {
									orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
										tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  ju, no_norm, true, false, OMP, 
										PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
								tempk += ku;
								tz0 += tzu;
							}
						}
					}
				}
			}
			else {

				int32_t tempi = 0, tempj = 0, tempk = 0, iu = 0, ju = 0, ku = 1;
				double txu = 0., tyu = 0., tzu = 0., tc = 0., tx0 = 0., ty0 = 0., tz0 = 0.;
				const bool skip = siddon_pre_loop_3D(bx, by, bz, x_diff, y_diff, z_diff, maxxx, maxyy, bzb, dx, dy, dz, Nx, Ny, Nz, tempi, tempj, tempk, 
					tyu, txu, tzu, Np, TYPE, detectors, tc, iu, ju, ku, tx0, ty0, tz0);

				if (skip)
					continue;

				double temp = 0.;

				if (attenuation_correction)
					LL = sqrt(x_diff * x_diff + y_diff * y_diff + z_diff * z_diff);
				const uint32_t tempi_a = tempi, tempj_a = tempj, tempk_a = tempk;
				const double ty0_a = ty0, tz0_a = tz0, tx0_a = tx0;
				uint32_t tempijk;
				if (crystal_size_z == 0.)
					tempijk = Nyx * static_cast<uint32_t>(tempk) + static_cast<uint32_t>(tempj) * Nx;
				else
					tempijk = static_cast<uint32_t>(tempj) * Nx;

				for (uint32_t ii = 0u; ii < Np; ii++) {
					if (tz0 < ty0 && tz0 < tx0) {
						if (attenuation_correction)
							compute_attenuation(tc, jelppi, LL, tz0, tempi, tempj, tempk, Nx, Nyx, atten);
						if (crystal_size_z == 0.) {
							orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
								tempj, local_sino, ax, osem_apu, no_norm, false, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							if (ku > 0)
								tempijk += Nyx;
							else
								tempijk -= Nyx;
						}
						else if (ii == Np - 1u) {
							orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
								tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, false, OMP, 
								PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
						}
						tempk += ku;
						tz0 += tzu;
						xyz = 3u;
					}
					else if (ty0 < tx0) {
						if (attenuation_correction)
							compute_attenuation(tc, jelppi, LL, ty0, tempi, tempj, tempk, Nx, Nyx, atten);
						if (crystal_size_z == 0.) {
							orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
								tempj, local_sino, ax, osem_apu, no_norm, false, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
						}
						else {
							orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
								tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, false, OMP, 
								PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
						}

						if (ju > 0) {
							tempijk += Nx;
						}
						else {
							tempijk -= Nx;
						}
						tempj += ju;
						ty0 += tyu;
						xyz = 2u;
					}
					else {
						if (attenuation_correction)
							compute_attenuation(tc, jelppi, LL, tx0, tempi, tempj, tempk, Nx, Nyx, atten);
						if (ii == Np - 1u) {
							if (crystal_size_z == 0.) {
								orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
									tempj, local_sino, ax, osem_apu, no_norm, false, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							else {
								orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
									tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, false, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
						}
						else {
							tempi += iu;
							tx0 += txu;
						}
						xyz = 1u;
					}
					Np_n++;
					if (tempj < 0 || tempi < 0 || tempk < 0 || tempi >= static_cast<int32_t>(Nx) || tempj >= static_cast<int32_t>(Ny) 
						|| tempk >= static_cast<int32_t>(Nz)) {
						if (ii != Np - 1u && (xyz == 1u || (xyz == 3u && crystal_size_z != 0.))) {
							if (xyz == 1u)
								tempi -= iu;
							else
								tempk -= ku;
							if (crystal_size_z == 0.) {
								orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
									tempj, local_sino, ax, osem_apu, no_norm, false, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							else {
								orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
									tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, false, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
						}
						break;
					}
				}

				temp = 1. / temp;
				ty0 = ty0_a;
				tx0 = tx0_a;
				tz0 = tz0_a;
				tempi = tempi_a;
				tempj = tempj_a;
				tempk = tempk_a;
				if (crystal_size_z == 0.)
					tempijk = Nyx * static_cast<uint32_t>(tempk) + static_cast<uint32_t>(tempj) * Nx;
				else
					tempijk = static_cast<uint32_t>(tempj) * Nx;
				if (attenuation_correction)
					temp *= exp(jelppi);
				if (normalization)
					temp *= norm_coef[lo];


				if (local_sino != 0.) {
					nominator_mfree(ax, local_sino, epps, temp, randoms_correction, randoms, lo);
					for (uint32_t ii = 0u; ii < Np_n; ii++) {
						if (tz0 < ty0 && tz0 < tx0) {
							if (crystal_size_z == 0.) {
								orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
									tempj, local_sino, ax, osem_apu, no_norm, true, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								if (ku > 0)
									tempijk += Nyx;
								else
									tempijk -= Nyx;
							}
							else if (ii == Np_n - 1u) {
								orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
									tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, true, false, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							tempk += ku;
							tz0 += tzu;
						}
						else if (ty0 < tx0) {
							if (crystal_size_z == 0.) {
								orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
									tempj, local_sino, ax, osem_apu, no_norm, true, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							else {
								orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
									tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, true, false, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}

							if (ju > 0) {
								tempijk += Nx;
							}
							else {
								tempijk -= Nx;
							}
							tempj += ju;
							ty0 += tyu;
						}
						else {
							if (ii == Np_n - 1u) {
								if (crystal_size_z == 0.) {
									orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
										tempj, local_sino, ax, osem_apu, no_norm, true, false, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
								else {
									orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
										tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, true, false, OMP, 
										PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
							}
							else {
								tempi += iu;
								tx0 += txu;
							}
						}
					}
				}
				else {
					for (uint32_t ii = 0u; ii < Np_n; ii++) {
						if (tz0 < ty0 && tz0 < tx0) {
							if (crystal_size_z == 0.) {
								orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
									tempj, local_sino, ax, osem_apu, no_norm, false, true, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								if (ku > 0)
									tempijk += Nyx;
								else
									tempijk -= Nyx;
							}
							else if (ii == Np_n - 1u) {
								orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
									tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, true, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							tempk += ku;
							tz0 += tzu;
						}
						else if (ty0 < tx0) {
							if (crystal_size_z == 0.) {
								orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
									tempj, local_sino, ax, osem_apu, no_norm, false, true, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}
							else {
								orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
									tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, true, OMP, 
									PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
							}

							if (ju > 0) {
								tempijk += Nx;
							}
							else {
								tempijk -= Nx;
							}
							tempj += ju;
							ty0 += tyu;
						}
						else {
							if (ii == Np_n - 1u) {
								if (crystal_size_z == 0.) {
									orth_distance_full(tempi, Nx, y_diff, x_diff, y_center[tempj], x_center, kerroin, length_, temp, tempijk, 1u,
										tempj, local_sino, ax, osem_apu, no_norm, false, true, OMP, PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
								else {
									orth_distance_3D_full(tempi, Nx, Nz, y_diff, x_diff, z_diff, y_center[tempj], x_center, z_center, temp, tempijk, 1u,
										tempj, tempk, local_sino, ax, osem_apu, detectors, Nyx, kerroin, dec,  iu, no_norm, false, true, OMP, 
										PRECOMPUTE, rhs, Summ, 0, elements, v_indices, idx);
								}
							}
							else {
								tempi += iu;
								tx0 += txu;
							}
						}
					}
				}
			}
		}
	}
}