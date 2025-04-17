function sigma = corfac(M, gamma, Twg, Tc_ns)
sigma = 1 / ((((1 / 2) * (Twg / Tc_ns) * (1 + (M ^ 2) * ((gamma - 1) / 2)) + (1 / 2)) ^ 0.68) * ((1 + (M ^ 2) * ((gamma - 1) / 2)) ^ 0.12));