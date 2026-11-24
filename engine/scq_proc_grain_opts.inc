/* scq_proc_grain: OPTS lines (a place with a pitch inlet that is listed here names tune= itself) */
	{ "grains",     "window=hann|gauss|tukey|blackman|expodec|rexpodec sync=async|sync reverse=0 " PITCH_OPTS },
	{ "glisson",    "window=hann|gauss|tukey|blackman|expodec|rexpodec" },
	{ "wavesets",   "mode=repeat|omit|reverse|shuffle|substitute|average" },
	{ "harmonicgrains", "chord=octaves|fifths|major|minor|harmonic|slendro|wholetone|cluster " PITCH_OPTS },
	{ "beatgrains", "burst=1 pattern=flat|up|down|octaves|fifths|random " PITCH_OPTS },
	{ "trainlet",   "chroma=0 " PITCH_OPTS },
	{ "gendy",      "dist=cauchy|logist|hyperbcos|arcsine|expon " PITCH_OPTS },
	{ "particles",  "dist=gauss|uniform|cauchy|expon|logist " PITCH_OPTS },
