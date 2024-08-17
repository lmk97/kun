const fs = require('node:fs');
const os = require('node:os');
const zlib = require('node:zlib');
const childProcess = require('node:child_process');

function getBuildType() {
    for (let i = 0, j = -1; i < process.argv.length; i++) {
        const name = process.argv[i];
        if (j === -1 && name.endsWith('.js')) {
            j = i + 1;
            continue;
        }
        if (j === -1) {
            continue;
        }
        if (name === '--clean') {
            return 'clean';
        }
        if (name === '--release') {
            return 'release';
        }
    }
    return 'debug';
}

function untar(buf) {
    const readString = (u8buf, offset, length) => {
        let result = '';
        for (let i = offset; i < offset + length; i++) {
            if (u8buf[i] !== 0) {
                result += String.fromCharCode(u8buf[i]);
            } else {
                break;
            }
        }
        return result;
    };
    const result = [];
    let offset = 0;
    while (offset < buf.byteLength) {
        let data;
        if (ArrayBuffer.isView(buf)) {
            data = new Uint8Array(buf.buffer, offset, 512);
        } else {
            data = new Uint8Array(buf, offset, 512);
        }
        if (data[0] === 0) {
            offset += 512;
            continue;
        }
        const header = {};
        header.name = readString(data, 0, 100);
        header.mode = readString(data, 100, 8);
        header.uid = readString(data, 108, 8);
        header.gid = readString(data, 116, 8);
        header.size = parseInt(readString(data, 124, 12), 8);
        header.mtime = readString(136, 12);
        header.chksum = readString(data, 148, 8);
        header.typeflag = readString(data, 156, 1);
        header.linkname = readString(data, 157, 100);
        header.magic = readString(data, 257, 6).trim();
        if (header.magic === 'ustar') {
            header.version = readString(data, 263, 2);
            header.uname = readString(data, 265, 32);
            header.gname = readString(data, 297, 32);
            header.devmajor = readString(data, 329, 8);
            header.devminor = readString(data, 337, 8);
            header.prefix = readString(data, 345, 155);
            if (header.prefix.length > 0) {
                header.name = header.prefix + header.name;
            }
        }
        offset += 512;
        if (header.typeflag === '0') {
            header.fileType = 'file';
            header.filePath = header.name;
            if (ArrayBuffer.isView(buf)) {
                header.fileData = new Uint8Array(buf.buffer, offset, header.size);
            } else {
                header.fileData = new Uint8Array(buf, offset, header.size);
            }
            offset += header.size;
            const remaining = 512 - (header.size % 512);
            if (remaining > 0 && remaining < 512) {
                offset += remaining;
            }
        } else if (header.typeflag === '5') {
            header.fileType = 'dir';
            header.filePath = header.name;
        }
        result.push(header);
    }
    return result;
}

async function ungzip(srcPath, dstPath) {
    return new Promise((resolve) => {
        const gz = zlib.createGunzip();
        const rs = fs.createReadStream(srcPath);
        const ws = fs.createWriteStream(dstPath);
        gz.on('error', (err) => {
            console.log(err);
            resolve(false);
        });
        ws.on('finish', () => {
            resolve(true);
        });
        ws.on('error', (err) => {
            console.log(err);
            resovle(false);
        });
        rs.pipe(gz).pipe(ws);
    });
}

async function fetchDeps(config) {
    for (const name in config.deps) {
        const { version, libs } = config.deps[name];
        let url = config.deps[name].url;
        if (url.endsWith('.git')) {
            url = url.substring(0, url.length - 4);
        }
        const urlPrefix = `${url}/releases/download/v${version}`;
        const depIncludeDir = `${config.includeDir}/${name}`;
        if (!fs.existsSync(depIncludeDir)) {
            const fileName = `${name}-include.tar.gz`;
            const targzPath = `${config.includeDir}/${fileName}`;
            if (!fs.existsSync(targzPath)) {
                const downloadUrl = `${urlPrefix}/${fileName}`;
                console.log(`Downloading '${downloadUrl}'`);
                const res = await fetch(downloadUrl);
                if (res.ok) {
                    const arrBuf = await res.arrayBuffer();
                    fs.writeFileSync(targzPath, new Uint8Array(arrBuf));
                } else {
                    console.log(`ERROR: fetch '${downloadUrl}'`);
                }
            }
            if (fs.existsSync(targzPath)) {
                const tarPath = targzPath.substring(0, targzPath.length - 3);
                console.log(`Ungzip      '${targzPath}' to '${tarPath}'`);
                const isSuccess = await ungzip(targzPath, tarPath);
                if (isSuccess) {
                    console.log(`Untar       '${tarPath}' to '${depIncludeDir}'`);
                    fs.mkdirSync(depIncludeDir);
                    const tarBuf = fs.readFileSync(tarPath);
                    untar(tarBuf).forEach((info) => {
                        let filePath = info.filePath;
                        if (filePath.startsWith('./')) {
                            filePath = filePath.substring(2);
                        }
                        filePath = `${depIncludeDir}/${filePath}`;
                        if (info.fileType === 'dir') {
                            fs.mkdirSync(filePath, { recursive: true });
                        } else {
                            fs.writeFileSync(filePath, info.fileData);
                        }
                    });
                } else {
                    console.log(`ERROR: Unzip '${targzPath}'`);
                    fs.unlinkSync(depIncludeDir);
                }
                fs.unlinkSync(tarPath);
                console.log(`Resolved    '${depIncludeDir}'`);
            }
        } else {
            console.log(`Exists      '${depIncludeDir}'`);
        }
        const { libPrefix, libSuffix } = config;
        const { arch, sub, vendor, sys, env } = config.triple;
        const triple = `${arch}${sub}-${vendor}-${sys}-${env}`;
        for (let i = 0; i < libs.length; i++) {
            const fileName = `${libs[i]}-lib-${triple}.gz`;
            const gzPath = `${config.libDir}/${fileName}`;
            const libPath = `${config.libDir}/${libPrefix}${libs[i]}${libSuffix}`;
            if (!fs.existsSync(gzPath)) {
                const downloadUrl = `${urlPrefix}/${fileName}`;
                console.log(`Downloading '${downloadUrl}'`);
                const res = await fetch(downloadUrl);
                if (res.ok) {
                    const arrBuf = await res.arrayBuffer();
                    fs.writeFileSync(gzPath, new Uint8Array(arrBuf));
                } else {
                    console.log(`ERROR: fetch '${downloadUrl}'`);
                }
            } else {
                console.log(`Exists      '${gzPath}'`);
            }
            if (fs.existsSync(gzPath)) {
                console.log(`Unzip       '${gzPath}' to ${libPath}`);
                const isSuccess = await ungzip(gzPath, libPath);
                if (isSuccess) {
                    console.log(`Resolved    '${libPath}'`);
                } else {
                    console.log(`ERROR: Ungzip '${gzName}'`);
                    fs.unlinkSync(libPath);
                }
            }
        }
    }
}

function findSourceFiles(dir, result) {
    let files = fs.readdirSync(dir);
    for (let i = 0; i < files.length; i++) {
        let path = `${dir}/${files[i]}`;
        const st = fs.statSync(path);
        if (st.isDirectory()) {
            findSourceFiles(path, result);
        } else if (path.endsWith('.cc')) {
            result.push(path);
        }
    }
}

function findIncludeFiles(sourceFilePath) {
    const result = [];
    let content = fs.readFileSync(sourceFilePath).toString();
    let index = sourceFilePath.lastIndexOf('.');
    if (index !== -1) {
        const includeFilePath = sourceFilePath.substring(0, index) + '.h';
        if (fs.existsSync(includeFilePath)) {
            content += '\n';
            content += fs.readFileSync(includeFilePath).toString();
        }
    }
    let prev = 0;
    for (let i = 0; i < content.length; i++) {
        if (content[i] !== '\n') {
            continue;
        }
        let str = content.substring(prev, i);
        if (str.startsWith('#include') && !str.includes('<')) {
            const begin = str.indexOf('"');
            const end = str.indexOf('"', begin + 1);
            if (begin != -1 && end != -1) {
                const path = str.substring(begin + 1, end);
                if (!result.includes(path)) {
                    result.push(path);
                }
            }
        }
        prev = i + 1;
    }
    return result;
}

function convertPath(path, platform) {
    if (platform === 'win32') {
        return path.replace(/\//g, '\\');
    } else {
        return path;
    }
}

function parseProjectObject(config) {
    let objSuffix = '';
    if (config.platform === 'win32') {
        objSuffix = '.obj';
    } else {
        objSuffix = '.o';
    }
    const result = {};
    const sourceFiles = [];
    findSourceFiles(config.srcDir, sourceFiles);
    for (let i = 0; i < sourceFiles.length; i++) {
        const includeFiles = findIncludeFiles(sourceFiles[i]);
        const includePaths = [];
        for (let j = 0; j < includeFiles.length; j++) {
            const includePath = `${config.srcDir}/${includeFiles[j]}`;
            if (fs.existsSync(includePath)) {
                includePaths.push(convertPath(includePath, config.platform));
            }
        }
        const subpath = sourceFiles[i].substring(config.srcDir.length);
        let index = subpath.lastIndexOf('.');
        if (index !== -1) {
            const objPath = config.distDir + subpath.substring(0, index) + objSuffix;
            index = objPath.lastIndexOf('/');
            if (index !== -1) {
                const objDir = objPath.substring(0, index);
                if (!fs.existsSync(objDir)) {
                    fs.mkdirSync(objDir, { recursive: true });
                }
            }
            const sourcePath = convertPath(sourceFiles[i], config.platform);
            result[sourcePath] = {
                includePaths: includePaths,
                objectPath: convertPath(objPath, config.platform)
            };
        }
    }
    return result;
}

function createMakefile(config) {
    const projectObject = parseProjectObject(config);
    let targetSuffix = '';
    let targetPath = '';
    let objectPaths = '';
    let libs = '';
    let includes = '';
    let cxxflags = '';
    let ldflags = '';
    if (config.platform === 'win32') {
        targetSuffix = '.exe';
    }
    targetPath = convertPath(
        `${config.distDir}/${config.target}${targetSuffix}`,
        config.platform
    );
    for (const sourcePath in projectObject) {
        objectPaths += projectObject[sourcePath].objectPath + ' ';
    }
    if (config.platform === 'win32') {
        libs += ' shlwapi.lib ws2_32.lib winmm.lib dbghelp.lib advapi32.lib';
        includes += ' /I' + convertPath(config.srcDir, config.platform);
        for (const name in config.deps) {
            const includePath = `${config.includeDir}/${name}`;
            includes += ' /I' + convertPath(includePath, config.platform);
            for (let i = 0; i < config.deps[name].libs.length; i++) {
                const libName = config.deps[name].libs[i];
                const libPath = `${config.libDir}/${config.libPrefix}${libName}${config.libSuffix}`;
                libs += ' ' + convertPath(libPath, config.platform);
            }
        }
        cxxflags += ' /std:c++17 /utf-8 /EHsc /wd4100 /wd4458 /wd4127 /nologo /DUNICODE /DWIN32_LEAN_AND_MEAN /DV8_COMPRESS_POINTERS';
        if (config.buildType === 'realease') {
            cxxflags += ' /O2';
        } else {
            cxxflags += ' /W4 /Od /Z7';
        }
        if (config.buildType === 'release') {
            ldflags += ' /RELEASE';
        } else {
            ldflags += ' /DEBUG';
        }
    } else {
        includes += ` -I ${config.srcDir}`;
        for (const name in config.deps) {
            includes += ` -I ${config.includeDir}/${name}`;
            for (let i = 0; i < config.deps[name].libs.length; i++) {
                const libName = config.deps[name].libs[i];
                const libPath = `${config.libDir}/${config.libPrefix}${libName}${config.libSuffix}`;
                libs += ' ' + libPath;
            }
        }
        cxxflags += ' -m64 -std=c++17 -march=native -mtune=native -Wpedantic -Wall -Wextra -Wno-unused-parameter -Wno-template-id-cdtor -DV8_COMPRESS_POINTERS';
        if (config.buildType === 'realease') {
            cxxflags += ' -O3';
        } else {
            cxxflags += ' -g -no-pie -Og';
        }
        ldflags += ' -rdynamic -flto';
    }
    let content = '';
    content += `${targetPath}: ${objectPaths}\n`;
    if (config.platform === 'win32') {
        content += `\tcl.exe /nologo ${libs} ${objectPaths} /Fe${targetPath} /link ${ldflags}\n`;
    } else {
        content += `\tg++ ${ldflags} -Wl,--start-group ${libs} ${objectPaths} -Wl,--end-group -o ${targetPath}\n`;
        if (config.buildType === 'release') {
            content += `\tstrip --strip-debug --strip-unneeded ${targetPath}\n`;
        }
    }
    for (const sourcePath in projectObject) {
        const objectPath = projectObject[sourcePath].objectPath;
        let includePaths = '';
        for (let i = 0; i < projectObject[sourcePath].includePaths.length; i++) {
            includePaths += projectObject[sourcePath].includePaths[i] + ' ';
        }
        content += `\n${objectPath}: ${sourcePath} ${includePaths}\n`;
        if (config.platform === 'win32') {
            content += `\tcl.exe ${cxxflags} ${includes} /c ${sourcePath} /Fo${objectPath}\n`;
        } else {
            content += `\tg++ -c ${cxxflags} ${includes} ${sourcePath} -o ${objectPath}\n`;
        }
    }
    fs.writeFileSync('./Makefile', content);
    console.log('Generate Makefile successfully!');
}

function buildProject(platform) {
    console.log('Building project...');
    if (platform === 'win32') {
        childProcess.execSync('nmake', {
            stdio: 'inherit'
        });
    } else {
        childProcess.execSync('make -j4', {
            stdio: 'inherit'
        });
    }
}

(async () => {

const config = {
    deps: {
        v8: {
            url: 'https://github.com/lmk97/build-v8',
            version: '12.4.254.19',
            libs: [ "v8" ]
        },
        openssl: {
            url: 'https://github.com/lmk97/build-openssl',
            version: '3.3.1',
            libs: [ "ssl", "crypto" ]
        },
        zlib: {
            url: 'https://github.com/lmk97/build-zlib',
            version: '1.3.1',
            libs: [ "zlib" ]
        }
    },
    target: 'kun',
    distDir: './dist',
    includeDir: './include',
    libDir: './lib',
    srcDir: './src'
};

config.triple = {};
const arch = os.arch();
if (arch === 'x64') {
    config.triple.arch = 'x86_64';
} else if (arch === 'arm64') {
    config.triple.arch = 'aarch64';
} else {
    throw new Error(`Unsupported arch '${arch}'`);
}
config.triple.sub = '';
const platform = os.platform();
if (platform === 'linux') {
    config.platform = 'linux';
    config.triple.vendor = 'unknown';
    config.triple.sys = 'linux';
    config.triple.env = 'gnu';
    config.libPrefix = 'lib';
    config.libSuffix = '.a';
} else if (platform === 'darwin') {
    config.platform = 'darwin';
    config.triple.vendor = 'apple';
    config.triple.sys = 'darwin';
    config.triple.env = '';
    config.libPrefix = 'lib';
    config.libSuffix = '.a';
} else if (platform === 'win32') {
    config.platform = 'win32';
    config.triple.vendor = 'pc';
    config.triple.sys = 'windows';
    config.triple.env = 'msvc';
    config.libPrefix = '';
    config.libSuffix = '.lib';
} else {
    throw new Error(`Unsupported platform '${platform}'`);
}

if (!fs.existsSync(config.distDir)) {
    fs.mkdirSync(config.distDir);
}

if (!fs.existsSync(config.includeDir)) {
    fs.mkdirSync(config.includeDir);
}

if (!fs.existsSync(config.libDir)) {
    fs.mkdirSync(config.libDir);
}

const buildType = getBuildType();
if (buildType === 'clean') {
    const files = fs.readdirSync(config.distDir);
    for (let i = 0; i < files.length; i++) {
        const path = `${config.distDir}/${files[i]}`;
        fs.rmSync(path, { recursive: true });
    }
    console.log('Clean successfully!');
    return;
} else {
    config.buildType = buildType;
}

await fetchDeps(config);

createMakefile(config);

buildProject(config.platform);

})();
