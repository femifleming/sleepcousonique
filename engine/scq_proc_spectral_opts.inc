/* scq_proc_spectral: OPTS lines, e.g. { "name", "mode=a|b" }, */
	{ "specgate",      "keep=loud|quiet" },
	{ "spectraldelay", "curve=rising|falling|random" },
	{ "specfilter",    "shape=comb|teeth|tilt|formants|harmonics " PITCH_OPTS },
	{ "harmonizer",    "chord=major|minor|fifths|octaves|sus|seventh|otonal|utonal detune=8 " PITCH_OPTS },
	{ "bassman",       "octave=one|two" },
