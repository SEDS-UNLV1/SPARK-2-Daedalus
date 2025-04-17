% This script is for heat transfer at discrete points on the MCCN

% Known Inputs Based on Coolant and MCCN Geometry
Cp = 0.7656; % 3.2053 KJ/KG*K [Btu/lb-F] Specific Heat at Constant Pressure
R_nz = 0.337; % 8.56 mm [in] Nozzle Radius of Curvature at Throat (NOT RADIUS OF THROAT - "Rn" in RPA)
A = 2.442; % 1575.62 mm2 [in^2] Area Along Chamber Axis - Change W/ Position
At = 2.442; % 1575.62 mm2 [in^2] Throat Cross Sectional Area
Mx = 1; % [-] Local Mach Number - Change W/ Position
M = 18913; % 18.913 kg/mol [lb/mol] Molecular Weight
Tc = 5682.474; % 3156.93 K [R] Combustion/Chamber Temp
Tc_ns = Tc; % [R] Nozzle Stagnation Temp/Chamber Temp
k = 27.39; % 367 W/m*k [Btu/(in^2)-s-F/in] Thermal Conductivity of Chamber Wall
mdot_c = 1.38; % .6261 kg/s [lb/s] Coolant Mass Flowrate
Cstar = 71822.835; % 1824.3 m/s [in/s] Characteristic Velocity
Twg = 1931.67; % 800 c [R] Gas-side Wall Temp (Assumed to be Critical Temp of Chamber Material)
Tco = 600; % [R] Coolant Bulk Temp (Assumed to be 600 R at throat) - Change W/ Position
t = 0.04; % 1 mm [in] Chamber Wall Thickness - Variable
g = 32.2; % [ft/s^2] Gravity
Dt = 1.763; % 44.79 mm [in] Diameter of Throat
gamma = 1.1957; % [-] Value Given by CEA
Pc_ns = 300; % [psi] Chamber Pressure
%L = ; % [in] Full Length of Regen Channel
%Wc = ; % [in] Cross-sectional Width of Regen Channel - Variable

% Outputs
Pr = (4 * gamma)/((9 * gamma) - 5); % [-] Prandtl Number Approximation
Mu = (46.6 * (10 ^ (-10))) * (M ^ 0.5) * (Tc ^ 0.6); % [lb/in-s] Viscosity
r = Pr ^ 0.33; % [-] Local Recovery Factor (Estimated, Exponent is 0.5 for Laminar, 0.33 for Turbulent)

Taw = adwall(Tc_ns, Mx, r, gamma); % [R] Adiabatic Wall Temp of Gas
sigma = corfac(M, gamma, Twg, Tc_ns); % [-] Correction Factor for Property Variations Across Boundary Layer
hg = httrns(At, A, Cstar, R_nz, Dt, Cp, Pc_ns, g, sigma, Pr, Mu); % [Btu/(in^2)-s-F] Overall Gas-side Heat-transfer Coefficient

q = hg * (Taw - Twg); % [Btu/(in^2)-s] Heat Flux
Twc = Twg - ((q * t) / k); % [R] Coolant-side Wall Temperature
hc = q / (Twc - Tco); % [Btu/(in^2)-s-F] Coolant-side Heat-transfer Coefficient