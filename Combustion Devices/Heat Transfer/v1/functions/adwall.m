function Taw = adwall(Tc_ns, Mx, r, gamma)
Taw = Tc_ns * ((1 + r * (Mx ^ 2) * ((gamma - 1) / 2))/(1 + (Mx ^ 2) * ((gamma - 1) / 2)));