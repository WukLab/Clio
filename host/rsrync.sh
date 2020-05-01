#!/bin/bash

src="ys@wuklab-02.sysnet.ucsd.edu:~/legomem/lego-mem/host/*"
rsync -a --exclude ".git" --exclude "*.o" -e "ssh -p 456" $src ./
