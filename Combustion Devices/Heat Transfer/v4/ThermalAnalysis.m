% This script is for heat transfer at discrete points on the MCCN

clear
clc
close all

% Import MCCN Coordinates off of RPA Export (MUST Edit .txt File, Remove
% Heading and Must Be Named "mccncoords.txt")

load mccncoords.txt -ascii
x = mccncoords(:,1);
y = mccncoords(:,2);

res = 30; % User-defined resolution for circles

% Generate evenly spaced points along contour
pt_x = linspace(min(x), max(x), res); % X-coordinates
pt_y = interp1(x, y, pt_x, 'linear'); % Y-coordinates

% Constants Based on Coolant Properties and MCCN Geometry
Cp = 0.6384; % [Btu/lb-F] Specific Heat at Constant Pressure
R_nz = 0.34; % [in] Nozzle Radius of Curvature at Throat (NOT RADIUS OF THROAT - "Rn" in RPA)
At = 2; % [in^2] Throat Cross Sectional Area
M = 0.0189966; % [lb/mol] Molecular Weight 
% Tcom = 5266.2722; % [R] Combustion/Chamber Temp (RPA Value)
% T_0 = Tcom; % [R] Nozzle Stagnation Temp/Chamber Temp
k = 7.61; % [Btu/(in^2)-s-F/in] Thermal Conductivity of Chamber Wall
% kc = ; % [Btu/(in^2)-s-F/in] Coolant Thermal Conductivity
% mdot_c = 1.38; % [lb/s] Coolant Mass Flowrate
Cstar = 71822.83; % [in/s] Characteristic Velocity
g = 32.2; % [ft/s^2] Gravity
Dt = 1.76; % [in] Diameter of Throat
gamma = 1.1849; % [-] Value Given by CEA (Also in RPA)
Pc_ns = 300; % [psi] Chamber Pressure

MxCFD = load('MachNumbers.txt');
TwgCFD = load('WallTemps.txt');
TcomCFD = load('MainLineTemps.txt');
xCFD = load('xLocations.txt');
TwgCFD = TwgCFD*1.8; % K to R conversion
TcomCFD = TcomCFD*1.8; % K to R conversion
xCFD = xCFD*39.37; % m to in conversion

xstat = zeros(1,res);
Mx = zeros(1,res);
Twg = zeros(1,res);
Tcom = zeros(1,res);


for i = 1:res
    ind = find(abs(xCFD - pt_x(i)) <= 1e-02);
    xstat(i) = ind(1);
    Mx(i) = MxCFD(xstat(i));
    Twg(i) = TwgCFD(xstat(i));
    Tcom(i) = TcomCFD(xstat(i));
end

mu = zeros(1,res);
Pr = zeros(1,res);
r = zeros(1,res);
Taw = zeros(1,res);
sigma = zeros(1,res);
A = zeros(1,res);
hg = zeros(1,res);
q_gas = zeros(1,res);

Fac1 = (gamma - 1)/2;

for i = 1:res
    mu(i) = 46.6e-10*(M^0.5)*(Tcom(i)^0.6);
    Pr(i) = (4*gamma)/(9*gamma - 5); %mu(i)*Cp/k;
    r(i) = Pr(i)^0.33; 
    Taw(i) = Tcom(i)*(1 + r(i)*Fac1*Mx(i)^2)/(1 + Fac1*Mx(i)^2);
    Den1 = (0.5*(Twg(i)/Tcom(i))*(1 + Fac1*M^2) + 0.5)^0.68;
    Den2 = (1 + Fac1*M^2)^0.12;
    sigma(i) = 1/(Den1*Den2);
    A(i) = pi*pt_y(i)^2;
    hg(i) = ((0.026/Dt^0.2)*((mu(i)^0.2)*Cp/(Pr(i)^0.6))*((Pc_ns*g/Cstar)^0.8)*((Dt/R_nz)^0.1))*((At/A(i))^0.9)*sigma(i);
    q_gas(i) = hg(i)*(Taw(i) - Twg(i)); %gas side heat transfer [Btu/(in^2) * s]
end



% Change with Position
% A = ; % [in^2] Area Along Chamber Axis - Change W/ Position
% Mx = ; % [-] Local Mach Number - Change W/ Position
% 
% Variables
% t = 0.039; % [in] Chamber Wall Thickness - Variable
% fw = ; % [in] Cooling Channel Fin Width - Variable
% ch = ; % [in] Channel Height - Variable
% cw = ; % [in] Channel Width - Variable
% cN = ; % [-] Estimate Number of Channels - Variable
% 
% Outputs
% mdot_ch = mdot_c / cN; % [lb/s] Individual Channel Mass Flowrate
% A_chan = ch * cw; % [in^2] Channel Cross-sectional Area
% S_c = 2 * (ch + cw); % [in] Cooling Channel Perimeter
% d_h = (4 * A_chan) / S_c; % [in] Channel Hydraulic Diameter
% Mu = (46.6 * (10 ^ (-10))) * (M ^ 0.5) * (Tcom ^ 0.6); % [lb/in-s] Coolant Viscosity - Change W/ Position
% 
% Pr_g = (4 * gamma)/((9 * gamma) - 5); % [-] Gas Prandtl Number Approximation
% Pr_c = ; % [-] Coolant Prandtl Number
% r = Pr_g ^ 0.33; % [-] Local Recovery Factor (Estimated, Exponent is 0.5 for Laminar, 0.33 for Turbulent)
% Re_c = (4 * mdot_ch) / (pi * d_h * Mu); % [-] Reynolds Number
% Nu_c = 0.023 * ((Re_c) ^ 0.8) * ((Pr_c) ^ 0.4); % [-] Nusselt Number
% 
% hc = (Nu_c * k) / d_h; % [Btu/(in^2)-s-F] Coolant-side Heat-transfer Coefficient
% m_ch = sqrt((2 * hc * fw) / k); % [-] Fin Effect Correction Factor
% eta_f = (tanh(m_ch / fw)) / (m_ch / fw); % [-] Another Fin Effect Correction Factor
% hc_corr = hc * ((cw + (2 * eta_f * ch)) / (cw + fw)); % [Btu/(in^2)-s-F] Corrected Coolant-side Heat-transfer Coefficient
% q = ; % [Btu/(in^2)-s] Heat Flux
% 
% Twg = ; % [R] Gas-side Wall Temp
% Taw = T_0 * ((1 + r * (Mx ^ 2) * ((gamma - 1) / 2))/(1 + (Mx ^ 2) * ((gamma - 1) / 2))); % [R] Adiabatic Wall Temp of Gas
% sigma = 1 / ((((1 / 2) * (Twg / T_0) * (1 + (M ^ 2) * ((gamma - 1) / 2)) + (1 / 2)) ^ 0.68) * ((1 + (M ^ 2) * ((gamma - 1) / 2)) ^ 0.12)); % [-] Correction Factor for Property Variations Across Boundary Layer
% hg = ((0.026 / (Dt ^ 0.2)) * (((Mu ^ 0.2) * Cp) / (Pr_c ^ 0.6)) * (((Pc_ns * g) / Cstar) ^ 0.8) * ((Dt / R_nz) ^ 0.1)) * ((At / A) ^ 0.9) * sigma; % [Btu/(in^2)-s-F] Overall Gas-side Heat-transfer Coefficient
% 
% Twc = Twg - ((q * t) / k); % [R] Coolant-side Wall Temperature
% 
% Itteration Loop
% for j = 1:res
% 
% end

% Plot the contour
% plot(x, y, 'LineWidth', 1.25)
% hold on
% plot(pt_x, pt_y, 'ro', 'MarkerFaceColor', 'r') % Aligned points
% grid on
% hold off
