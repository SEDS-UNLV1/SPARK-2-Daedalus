function init_pmsm(model, setSolver)
% Safe init for PMSM FOC+SVPWM + Pump load params
if nargin < 1 || isempty(model), model = bdroot; end
if nargin < 2, setSolver = false; end

%% ---------- Design choices ----------
fSVPWM   = 10e3;                  % Hz
%% Ts_ctrl  = 1/fSVPWM;              % s (current loop & SVPWM)
Ts_ctrl = 2.5e-5;
doubleUpdate = false;
Ts_svpwm = doubleUpdate * Ts_ctrl/2 + ~doubleUpdate * Ts_ctrl;
Ts_speed = 1e-4;                  % s (speed loop)

% Motor parameters
Vdc       = 48;
poles     = 8;                    % # of poles (p = pole pairs below)
Rs        = 0.30;
Ld        = 0.6e-3;
Lq        = 0.6e-3;
lambda_m  = 0.035;
J         = 3e-4;
B_mech    = 2e-4;                 % viscous friction in motor (mechanical)
p = poles/2
K_lambda = 2/(3*p*lambda_m);
% Pump-load model parameters (the ones you asked about)
K_pump    = 1e-4;                 % N·m·s^2 (quadratic torque coeff)
B_pump    = 1e-3;                 % N·m·s   (viscous)
Tc_pump   = 1e-2;                 % N·m     (Coulomb)
I_rated   = 2200;
% Stimuli
w_cmd_rps = 2*pi*50;
Tload     = 0.0;

%% ---------- Promote to Simulink.Parameter ----------
% motor
Vdc=Simulink.Parameter(Vdc);  poles=Simulink.Parameter(poles);
p_obj = Simulink.Parameter(poles.Value/2);       % pole pairs
Rs=Simulink.Parameter(Rs);    Ld=Simulink.Parameter(Ld);
Lq=Simulink.Parameter(Lq);    lambda_m=Simulink.Parameter(lambda_m);
J =Simulink.Parameter(J);     B_mech =Simulink.Parameter(B_mech);

fSVPWM   = Simulink.Parameter(fSVPWM);
Ts_ctrl  = Simulink.Parameter(Ts_ctrl);
Ts_svpwm = Simulink.Parameter(Ts_svpwm);
Ts_speed = Simulink.Parameter(Ts_speed);

% pump-load
K_pump = Simulink.Parameter(K_pump);
B_pump = Simulink.Parameter(B_pump);
Tc_pump= Simulink.Parameter(Tc_pump);

% stimuli
w_cmd_rps=Simulink.Parameter(w_cmd_rps);
Tload    = Simulink.Parameter(Tload);

% Make items the model can "see" (if you use names in block dialogs)
assignin('base','lambda_m', lambda_m.Value);
assignin('base','p',        p_obj.Value);
assignin('base','K_pump',   K_pump);
assignin('base','B_pump',   B_pump);
assignin('base','Tc_pump',  Tc_pump);
assignin('base', 'K_lambda', K_lambda);
assignin('base', 'Ts_ctrl', Ts_ctrl.Value);

%% ---------- Solver (only when allowed; PostLoadFcn/InitFcn) ----------
if setSolver
    try, set_param(model,'FastRestart','off'); end
    set_param(model,'StopTime','10', ...
        'SolverType','Fixed-step','Solver','ode3', ...
        'FixedStep', num2str(Ts_ctrl.Value/10));
end

%% ---------- Block paths (edit to match your hierarchy) ----------
blk_pwm     = [model '/Power-Electronics + Motor/Controller/SVPWM Generator (2-Level)'];
blk_speed_ctrl = [model '/Power-Electronics + Motor/Controller/Speed Control'];
blk_rate_iq = [model '/Power-Electronics + Motor/Controller/Rate Transition Current 1'];
blk_rate_id = [model '/Power-Electronics + Motor/Controller/Rate Transition Current 2'];
blk_rate_sp = [model '/Power-Electronics + Motor/Controller/Speed Control/Rate Transition Speed 1'];
blk_dc      = [model '/Power-Electronics + Motor/Plant/DC Bus'];
blk_motor   = [model '/Power-Electronics + Motor/Plant/PMSM'];
blk_cd_conv = [model '/Power-Electronics + Motor/C-D'];

% Pump-load subsystem gain blocks
blk_load_B  = [model '/Power-Electronics + Motor/Plant/Pump Load Torque/ViscDamp'];
blk_load_K  = [model '/Power-Electronics + Motor/Plant/Pump Load Torque/PumpCoeff'];
blk_load_Tc = [model '/Power-Electronics + Motor/Plant/Pump Load Torque/Tcoul'];

%% ---------- Push parameters into blocks ----------
% SVPWM

try
    set_param(blk_pwm, 'PWMfrequency', num2str(fSVPWM.Value), ...
                        'Ts',          num2str(Ts_svpwm.Value));
end



% Rate transitions
try, set_param(blk_rate_iq,'outPortSampleTime',num2str(Ts_ctrl.Value));  end
try, set_param(blk_rate_id,'outPortSampleTime',num2str(Ts_ctrl.Value));  end
try, set_param(blk_rate_sp,'outPortSampleTime',num2str(Ts_speed.Value)); end
try, set_param(blk_speed_ctrl, 'Gain', K_lambda);end

% Suppose you have N Rate Transition blocks to configure
N = 8;   % change this to however many you actually have

for i = 1:N
    blkname = sprintf('Rate Transition%d', i);
    try
        % Example: set Rate Transition block parameter
        set_param(blk_cd_conv, blkname, num2str(Ts_ctrl.Value));
    end
    try
        % Update the outPortSampleTime parameter of each Rate Transition{i}
        set_param([blk_cd_conv '/Rate Transition' num2str(i)], ...
                  'outPortSampleTime', num2str(Ts_speed.Value));
    end
    try
        set_param([blk_cd_conv '/Rate Transition' num2str(i)], ...
                  'outPortSampleTime', num2str(Ts_ctrl.Value));
    end
    try
        set_param([blk_cd_conv '/Rate Transition' num2str(i)], ...
                  'outPortSampleTime', num2str(Ts_speed.Value));
    end
end

% DC bus
try, set_param(blk_dc,'Amplitude',num2str(Vdc.Value)); end

% PMSM mask (names may vary slightly by block variant)
try
    set_param(blk_motor, ...
        'Resistance',     num2str(Rs.Value), ...
        'dqInductances',  sprintf('[%g %g]', Ld.Value, Lq.Value), ...
        'Flux',           num2str(lambda_m.Value), ...
        'PolePairs',      num2str(p_obj.Value), ...
        'Inertia',        num2str(J.Value), ...
        'MeasurementBus', true, ...
        'Damping',        num2str(B_mech.Value));
end

% --- Pump-load Gains (this is what you asked for) ---
% You can either pass the **names** (so they stay live/tunable) …
try
    set_param(blk_load_B,  'Gain', 'B_pump');
    set_param(blk_load_K,  'Gain', 'K_pump');
    set_param(blk_load_Tc, 'Gain', 'Tc_pump');
end
% …or, if you prefer, push numeric strings:
% set_param(blk_load_B,  'Gain', num2str(B_pump.Value));
% set_param(blk_load_K,  'Gain', num2str(K_pump.Value));
% set_param(blk_load_Tc, 'Gain', num2str(Tc_pump.Value));

%% ---------- Stimuli ----------
t = (0:Ts_ctrl.Value:10)'; 
assignin('base','w_ref_ts', timeseries(w_cmd_rps.Value*ones(size(t)),t));
assignin('base','Tload_ts', timeseries(Tload.Value*ones(size(t)),t));

disp('[init_pmsm] Initialization complete.');
end
