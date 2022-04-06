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

