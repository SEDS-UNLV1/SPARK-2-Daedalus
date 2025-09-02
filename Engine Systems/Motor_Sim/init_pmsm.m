function init_pmsm(model)
% INIT script for PMSM FOC+SVPWM model for the electric pump fed engine in
% development as part of the Daedulus project at SEDS UNLV

if nargin < 1, model = 'pmsm_motor'; end

%% ====== Design choices (one-stop edit) ======

fSVPWM      = 10e3;          % Hz, inverter switching frequency

Ts_ctrl   = 1/fSVPWM;        % base rate for controller & SVPWM generator sample time, 
                                % inverse prop. to fPWM because of controller updates once per PWM period
doubleUpdate = false;        % set true if controller updates twice per period

if doubleUpdate, Ts_svpwm = Ts_ctrl/2; else, Ts_svpwm = Ts_ctrl; end

Ts_speed  = 1e-3;          % speed PI sampling rate (5–10x slower than current loop). Speed dynamics are much slower
Ts_scope  = Ts_ctrl;       % logging decimation (in order to prevent gigabytes of data)

Vdc       = 48;            % DC bus
poles     = 8;             % # of poles
Rs        = 0.30;          % stator phase resistance (ohm)
Ld        = 0.6e-3;        % H
Lq        = 0.6e-3;        % H
lambda_m  = 0.035;         % Wb (PM flux)
J         = 3e-4;          % kg·m^2
B         = 2e-4;          % N·m·s/rad

w_cmd_rps = 2*pi*50;       % speed command (rad/s)
Tload     = 0.0;           % constant load torque (N·m)

%% ====== Promote to tunable Simulink.Parameter (nice for code gen) ======
makeParam = @(v,u) setfield(Simulink.Parameter(v),'CoderInfo',coder.asap2('Min','-Inf','Max','Inf')); %#ok<SFLD>
Vdc    = makeParam(Vdc,'V');           %#ok<NASGU>
fSVPWM   = makeParam(fSVPWM,'Hz');         %#ok<NASGU>
Ts_ctrl = makeParam(Ts_ctrl,'s');      %#ok<NASGU>
Ts_svpwm = makeParam(Ts_svpwm,'s');    %#ok<NASGU>
Ts_speed = makeParam(Ts_speed,'s');    %#ok<NASGU>

% Motor params
Rs = Simulink.Parameter(Rs);   Ld = Simulink.Parameter(Ld);
Lq = Simulink.Parameter(Lq);   lambda_m = Simulink.Parameter(lambda_m);
J  = Simulink.Parameter(J);    B  = Simulink.Parameter(B);
poles = Simulink.Parameter(poles);

w_cmd_rps = Simulink.Parameter(w_cmd_rps);
Tload     = Simulink.Parameter(Tload);

%% ====== Configure model solver to play nice with power electronics ======
open_system(model);  % no-op if already open
set_param(model, ...
    'StopTime', '10', ...
    'SolverType', 'Fixed-step', ...
    'Solver',    'ode3', ...            % or 'ode4' / 'ode14x' for power electronics
    'FixedStep', num2str(Ts_ctrl/10));  % plant step 10–20x smaller than Ts_ctrl

%% ====== Push values into specific blocks ======
% Adjust these paths to match your hierarchy
blk_pwm   = [model '/Controller/FOC Controller/SVPWM Generator (2-Level)'];
blk_rate1 = [model '/Controller/FOC Controller/Rate Transition (current loop)'];
blk_rate2 = [model '/Controller/FOC Controller/Rate Transition (speed loop)'];
blk_dc    = [model '/Plant/DC Bus'];
blk_motor = [model '/Plant/PMSM'];
blk_load  = [model '/Plant/Pump Load Torque'];

% SVPWM block
set_param(blk_pwm, ...
    'PWMfrequency',  num2str(fSVPWM.Value), ...
    'Ts',            num2str(Ts_svpwm.Value));   % "Sample time" field

% Rate transitions
set_param(blk_rate1, 'outPortSampleTime', num2str(Ts_ctrl.Value));
set_param(blk_rate2, 'outPortSampleTime', num2str(Ts_speed.Value));

% DC bus & load
set_param(blk_dc,   'Vdc',   'Vdc');             % assuming mask param named Vdc
set_param(blk_load, 'Tload', 'Tload');           % adapt to your mask

% Motor block (names follow Simscape PMSM masks—adjust to yours)
set_param(blk_motor, ...
   'Rs', 'Rs', 'Ld', 'Ld', 'Lq', 'Lq', ...
   'lambda_m', 'lambda_m', 'J', 'J', 'B', 'B', 'p', 'poles');

%% ====== Stimuli (speed profile / load pulse) ======
% Example: a speed step and a load step as timeseries in base workspace
t  = (0:Ts_ctrl.Value:10)';                        %#ok<NASGU>
w  = w_cmd_rps.Value * ones(size(t));              %#ok<NASGU>
T  = Tload.Value * ones(size(t));                  %#ok<NASGU>
assignin('base','w_ref_ts', timeseries(w,t));
assignin('base','Tload_ts', timeseries(T,t));

disp('[init_pmsm] Initialization complete.');
end
