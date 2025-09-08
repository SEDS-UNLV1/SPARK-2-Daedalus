% Suppose your speed PI loop sample time is 1e-3 s
Ts_speed = 1e-3;

% Frequencies of interest (rad/s). Example: 0.5 Hz – 50 Hz mech ≈ 3–300 rad/s
w = logspace(0, 2.5, 30);  % 30 points between 1 and ~300 rad/s

% Build the sinestream
in_sine1 = frest.Sinestream('Frequency', w, ...
                            'Amplitude', 0.01, ...   % adjust ~1–5% of rated input
                            'SamplesPerPeriod', 40, ...
                            'NumPeriods', 4, ...
                            'Ts', Ts_speed);          % <--- this sets the sample time
