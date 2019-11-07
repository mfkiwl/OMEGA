function output = loadInterfile(filename)
%LOADINTERFILE Loads the input file as a MATLAB matrix with the same
%   dimensions and format type as the original.
%
%   Header file must exist for the file load to work and must contain the
%   number format, number of bytes per pixel and the each individual matrix
%   dimensions. Up to five dimensions are supported natively.
%
%   Supported file types are hdr, img, h33 and i33.
%
% Example:
%   loadInterfile(filename)
%
% Input:
%   filename = Name of the image (.img or .i33) or header file (.hdr or
%   .h33)
%
tline = 0;
ll = 1;
M = cell(1,1);
if strcmp(filename(end-2:end),'img') || strcmp(filename(end-2:end),'i33')
    filename = [filename(1:end-3) 'hdr'];
end
fid = fopen(filename);
if fid == -1
    filename = [filename(1:end-3) 'h33'];
    fid = fopen(filename);
    if fid == -1
        error(['Could not find either ' filename ' or ' [filename(1:end-3) 'hdr']])
    end
end
while sum(tline ~= -1) >= 1 || isempty(tline ~= -1)
    tline = fgetl(fid);
    M{ll} = {tline};
    ll = ll + 1;
end
fclose(fid);
M = M(1:end-1);
n_dim1 = 1;
n_dim2 = 1;
n_dim3 = 1;
n_dim4 = 1;
n_dim5 = 1;

for kk = 1 : length(M)
    if ~cellfun('isempty',strfind(M{kk},'number format'))
        if ~cellfun('isempty',strfind(M{kk},'float')) || ~cellfun('isempty',strfind(M{kk},'short float'))
            type = 'single';
        elseif ~cellfun('isempty',strfind(M{kk},'double')) || ~cellfun('isempty',strfind(M{kk},'long float'))
            type = 'double';
        elseif ~cellfun('isempty',strfind(M{kk},'signed integer'))
            for hh = 1 : length(M)
                if ~cellfun('isempty',strfind(M{hh},'number of bytes per pixel'))
                    apu = cell2mat(M{hh});
                    if strcmp(apu(end), '1')
                        type = 'int8';
                    elseif strcmp(apu(end), '2')
                        type = 'int16';
                    elseif strcmp(apu(end), '4')
                        type = 'int32';
                    elseif strcmp(apu(end), '8')
                        type = 'int64';
                    else
                        error('Cannot read number of bytes per pixel')
                    end
                    break
                end
            end
        elseif ~cellfun('isempty',strfind(M{kk},'unsigned integer'))
            for hh = 1 : length(M)
                if ~cellfun('isempty',strfind(M{hh},'number of bytes per pixel'))
                    apu = cell2mat(M{hh});
                    if strcmp(apu(end), '1')
                        type = 'uint8';
                    elseif strcmp(apu(end), '2')
                        type = 'uint16';
                    elseif strcmp(apu(end), '4')
                        type = 'uint32';
                    elseif strcmp(apu(end), '8')
                        type = 'uint64';
                    else
                        error('Cannot read number of bytes per pixel')
                    end
                    break
                end
            end
        end
%     elseif ~cellfun('isempty',strfind(M{kk},'number of dimensions'))
%         apu = cell2mat(M{kk});
%         n_dim = str2double(apu(end));
    elseif ~cellfun('isempty',strfind(M{kk},'matrix size[1]'))
        koko = length('matrix size[1]:=');
        apu = cell2mat(M{kk});
        n_dim1 = str2double(apu(koko+1:end));
    elseif ~cellfun('isempty',strfind(M{kk},'matrix size[2]'))
        koko = length('matrix size[2]:=');
        apu = cell2mat(M{kk});
        n_dim2 = str2double(apu(koko+1:end));
    elseif ~cellfun('isempty',strfind(M{kk},'matrix size[3]'))
        koko = length('matrix size[3]:=');
        apu = cell2mat(M{kk});
        n_dim3 = str2double(apu(koko+1:end));
    elseif ~cellfun('isempty',strfind(M{kk},'matrix size[4]'))
        koko = length('matrix size[4]:=');
        apu = cell2mat(M{kk});
        n_dim4 = str2double(apu(koko+1:end));
    elseif ~cellfun('isempty',strfind(M{kk},'matrix size[5]'))
        koko = length('matrix size[5]:=');
        apu = cell2mat(M{kk});
        n_dim5 = str2double(apu(koko+1:end));
    end
end


if strcmp(filename(end-2:end),'hdr') || strcmp(filename(end-2:end),'h33')
    filename = [filename(1:end-3) 'img'];
end

fid = fopen(filename);
if fid == - 1
    filename = [filename(1:end-3) 'i33'];
    fid = fopen(filename);
    if fid == -1
        error(['Could not find either ' filename ' or ' [filename(1:end-3) 'img']])
    end
end
output = fread(fid, inf, [type '=>' type]);
output = reshape(output, n_dim1, n_dim2, n_dim3, n_dim4, n_dim5);
fclose(fid);