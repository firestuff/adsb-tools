## Overview

sourcestats takes an ADS-B feed and periodically outputs statistics on message and clock rate (written in Python)

These statistics will appear in the adsbus logs.

## Use

Pass `--exec-send=json="exec /path/to/sourcestats.py"` to [adsbus](../../adsbus/).
