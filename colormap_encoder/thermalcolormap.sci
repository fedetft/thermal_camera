// Copyright (C) 2022 by Terraneo Federico
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>

// Load into Scilab with 'exec thermalcolormap.sci'
// fprintfMat('thermalcolormap',round(255*thermalcolormap(256)),'%.0f')

function [cmap]=thermalcolormap(n)
    cmap=[];
    for i=0:n-1
        cmap=[cmap; tcmapsample(i/(n-1))];
    end
endfunction

function [result]=tcmapsample(x)
    if x<=0.2 then
        y=x;
        result=[0 0 y/0.2];
    elseif x<=0.4 then
        y=x-0.2;
        result=[0 y/0.2 1];
    elseif x<=0.6 then
        y=x-0.4;
        result=[y/0.2 1 1-(y/0.2)];
    elseif x<=0.8 then
        y=x-0.6;
        result=[1 1-(y/0.2) 0];
    elseif x<=1.0 then
        y=x-0.8;
        result=[1 y/0.2 y/0.2]
    else
        result=[1 1 1];
    end
endfunction
