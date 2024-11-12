const fs = require('node:fs');
const os = require('node:os');
const zlib = require('node:zlib');
const childProcess = require('node:child_process');

function untar(buf) {
    const readString = (u8Arr, offset, length) => {
        let result = '';
        for (let i = offset; i < offset + length; i++) {
            if (u8Arr[i] !== 0) {
                result += String.fromCharCode(u8Arr[i]);
            } else {
                break;
            }
        }
        return result;
    };
    let buffer;
    let offset;
    if (ArrayBuffer.isView(buf)) {
        buffer = buf.buffer;
        offset = buf.byteOffset;
    } else {
        buffer = buf;
        offset = 0;
    }
    const u8Arr = new Uint8Array(buffer, offset, buf.byteLength);
    const result = [];
    offset = 0;
    while (offset < u8Arr.byteLength) {
        const data = u8Arr.subarray(offset, offset + 512);
        offset += 512;
        if (data[0] === 0) {
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
            header.name = header.prefix + header.name;
        }
        if (header.typeflag === '0') {
            header.fileType = 'file';
            header.filePath = header.name;
            header.fileData = u8Arr.subarray(offset, offset + header.size);
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
        rs.on('error', (err) => {
            console.log(err);
            resolve(false);
        });
        ws.on('error', (err) => {
            console.log(err);
            resovle(false);
        });
        ws.on('finish', () => {
            resolve(true);
        });
        rs.pipe(gz).pipe(ws);
    });
}

function getBuildType() {
    for (let i = 0, j = -1; i < process.argv.length; i++) {
        const name = process.argv[i];
        if (j === -1) {
            if (name.endsWith('.js')) {
                j = i + 1;
            }
            continue;
        }
        if (name === '--clean') {
            return 'clean';
        }
        if (name === '--release') {
            return 'release';
        }
        if (name === '--debug') {
            return 'debug';
        }
        console.log(`bad option '${name}'`);
    }
    return 'debug';
}

async function fetchDeps(config) {
    for (const name in config.deps) {
        const { url, version, libs } = config.deps[name];
        const urlPrefix = `${url}/releases/download/v${version}`;
        const includeDir = `${config.includeDir}/${name}`;
        if (!fs.existsSync(includeDir)) {
            const filename = `${name}-include.tar.gz`;
            const targzPath = `${config.includeDir}/${filename}`;
            if (!fs.existsSync(targzPath)) {
                const downloadUrl = `${urlPrefix}/${filename}`;
                console.log(`Download '${downloadUrl}'`);
                const res = await fetch(downloadUrl);
                if (res.ok) {
                    const arrBuf = await res.arrayBuffer();
                    fs.writeFileSync(targzPath, new Uint8Array(arrBuf));
                } else {
                    console.log(`ERROR: fetch '${downloadUrl}'`);
                }
            }
        }
        const { arch, sub, vendor, sys, env } = config.triple;
        const triple = `${arch}${sub}-${vendor}-${sys}-${env}`;
        for (let i = 0; i < libs.length; i++) {
            const filename = `${libs[i]}-lib-${triple}.gz`;
            const gzPath = `${config.libDir}/${filename}`;
            if (!fs.existsSync(gzPath)) {
                const downloadUrl = `${urlPrefix}/${filename}`;
                console.log(`Download '${downloadUrl}'`);
                const res = await fetch(downloadUrl);
                if (res.ok) {
                    const arrBuf = await res.arrayBuffer();
                    fs.writeFileSync(gzPath, new Uint8Array(arrBuf));
                } else {
                    console.log(`ERROR: fetch '${downloadUrl}'`);
                }
            }
        }
    }
}

async function ungzipDeps(config) {
    for (const name in config.deps) {
        const includeDir = `${config.includeDir}/${name}`;
        if (!fs.existsSync(includeDir)) {
            const filename = `${name}-include.tar.gz`;
            const targzPath = `${config.includeDir}/${filename}`;
            if (!fs.existsSync(targzPath)) {
                console.log(`ERROR: ${targzPath} not found`);
                break;
            }
            const tarPath = targzPath.substring(0, targzPath.length - 3);
            console.log(`Ungzip   '${targzPath}' => '${tarPath}'`);
            const success = await ungzip(targzPath, tarPath);
            if (success) {
                console.log(`Untar    '${tarPath}' => '${includeDir}'`);
                fs.mkdirSync(includeDir);
                const tarBuf = fs.readFileSync(tarPath);
                untar(tarBuf).forEach((info) => {
                    let filePath = info.filePath;
                    if (filePath.startsWith('./')) {
                        filePath = filePath.substring(2);
                    }
                    filePath = `${includeDir}/${filePath}`;
                    if (info.fileType === 'dir') {
                        fs.mkdirSync(filePath, { recursive: true });
                    } else {
                        fs.writeFileSync(filePath, info.fileData);
                    }
                });
                console.log(`Resolved '${includeDir}'`);
            } else {
                fs.unlinkSync(includeDir);
                console.log(`ERROR: Ungzip '${targzPath}'`);
            }
            fs.unlinkSync(tarPath);
        } else {
            console.log(`Exists   '${includeDir}'`);
        }
        const libs = config.deps[name].libs;
        const { libPrefix, libSuffix } = config;
        const { arch, sub, vendor, sys, env } = config.triple;
        const triple = `${arch}${sub}-${vendor}-${sys}-${env}`;
        for (let i = 0; i < libs.length; i++) {
            const filename = `${libs[i]}-lib-${triple}.gz`;
            const gzPath = `${config.libDir}/${filename}`;
            const libPath = `${config.libDir}/${libPrefix}${libs[i]}${libSuffix}`;
            if (fs.existsSync(gzPath)) {
                console.log(`Ungzip   '${gzPath}' => '${libPath}'`);
                const success = await ungzip(gzPath, libPath);
                if (success) {
                    console.log(`Resolved '${libPath}'`);
                } else {
                    console.log(`ERROR: Ungzip '${gzName}'`);
                    fs.unlinkSync(libPath);
                }
            }
        }
    }
}

function findSourcePaths(dir, result) {
    const names = fs.readdirSync(dir);
    for (let i = 0; i < names.length; i++) {
        const path = `${dir}/${names[i]}`;
        const st = fs.statSync(path);
        if (st.isDirectory()) {
            findSourcePaths(path, result);
        } else if (path.endsWith('.cc')) {
            result.push(path);
        }
    }
}

function findHeaderPaths(sourcePath) {
    const result = [];
    let content = fs.readFileSync(sourcePath).toString();
    const index = sourcePath.lastIndexOf('.');
    if (index !== -1) {
        const headerPath = sourcePath.substring(0, index) + '.h';
        if (fs.existsSync(headerPath)) {
            content += '\n';
            content += fs.readFileSync(headerPath).toString();
        }
    }
    let prev = 0;
    for (let i = 0; i < content.length; i++) {
        if (content[i] !== '\n') {
            continue;
        }
        let line = content.substring(prev, i);
        if (line.startsWith('#include') && !line.includes('<')) {
            const begin = line.indexOf('"');
            let end = -1;
            if (begin !== -1) {
                end = line.indexOf('"', begin + 1);
            }
            if (begin !== -1 && end !== -1) {
                const path = line.substring(begin + 1, end);
                if (!result.includes(path)) {
                    result.push(path);
                }
            }
        }
        prev = i + 1;
    }
    return result;
}

function parseProject(config) {
    const { platform, srcDir, distDir } = config;
    const objSuffix = platform === 'win32' ? '.obj' : '.o';
    const result = {};
    const sourcePaths = [];
    findSourcePaths(srcDir, sourcePaths);
    for (const sourcePath of sourcePaths) {
        const sourceHeaderPaths = findHeaderPaths(sourcePath);
        const headerPaths = [];
        for (const headerPath of sourceHeaderPaths) {
            const path = `${srcDir}/${headerPath}`;
            if (fs.existsSync(path)) {
                headerPaths.push(path);
            }
        }
        const suffix = sourcePath.substring(srcDir.length);
        let index = suffix.lastIndexOf('.');
        if (index === -1) {
            continue;
        }
        const objPath = distDir + suffix.substring(0, index) + objSuffix;
        index = objPath.lastIndexOf('/');
        if (index !== -1) {
            const objDir = objPath.substring(0, index);
            if (!fs.existsSync(objDir)) {
                fs.mkdirSync(objDir, { recursive: true });
            }
        }
        result[sourcePath] = {
            headerPaths: headerPaths,
            objectPath: objPath
        };
    }
    return result;
}

function toWinPath(path) {
    return path.replace(/\//g, '\\');
}

function generateWinMakefile(config) {
    const projectObject = parseProject(config);
    const targetPath = toWinPath(`${config.distDir}/${config.target}.exe`);
    let objectPaths = '';
    let libs = '';
    let includes = '';
    let cxxflags = '';
    let ldflags = '';
    for (const key in projectObject) {
        const objPath = toWinPath(projectObject[key].objectPath);
        objectPaths += objPath + ' ';
    }
    libs += 'shlwapi.lib ws2_32.lib userenv.lib ';
    libs += 'winmm.lib dbghelp.lib advapi32.lib ';
    includes += `/I ${toWinPath(config.srcDir)} `;
    const { libDir, libPrefix, libSuffix } = config;
    for (const name in config.deps) {
        const includeDir = `${config.includeDir}/${name}`;
        includes += `/I ${toWinPath(includeDir)} `;
        for (const libName of config.deps[name].libs) {
            const libPath = `${libDir}/${libPrefix}${libName}${libSuffix}`;
            libs += toWinPath(libPath) + ' ';
        }
    }
    cxxflags += '/std:c++17 /utf-8 /EHsc /nologo ';
    cxxflags += '/Zc:__cplusplus /Zc:preprocessor ';
    cxxflags += '/wd4100 /wd4127 /wd4458 ';
    cxxflags += '/DUNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN ';
    cxxflags += '/DV8_COMPRESS_POINTERS ';
    if (config.buildType === 'release') {
        cxxflags += '/O2 /RELEASE ';
    } else {
        cxxflags += '/W4 /Od /Z7 /DEBUG ';
    }
    let content = '';
    content += `${targetPath}: ${objectPaths}\n`;
    content += `\tcl.exe /nologo ${libs} ${objectPaths} /Fe${targetPath} /link ${ldflags}\n`;
    for (const key in projectObject) {
        const sourcePath = toWinPath(key);
        const objectPath = toWinPath(projectObject[key].objectPath);
        let headerPaths = '';
        for (const headerPath of projectObject[key].headerPaths) {
            headerPaths += toWinPath(headerPath) + ' ';
        }
        content += `\n${objectPath}: ${sourcePath} ${headerPaths}\n`;
        content += `\tcl.exe ${cxxflags} ${includes} /c /Tp${sourcePath} /Fo${objectPath}\n`;
    }
    fs.writeFileSync('./Makefile', content);
    console.log('Generate Makefile successfully!');
}

function generateUnixMakefile(config) {
    const projectObject = parseProject(config);
    const targetPath = `${config.distDir}/${config.target}`;
    let objectPaths = '';
    let libs = '';
    let includes = '';
    let cxxflags = '';
    let ldflags = '';
    for (const key in projectObject) {
        objectPaths += projectObject[key].objectPath + ' ';
    }
    includes += `-I ${config.srcDir} `;
    const { libDir, libPrefix, libSuffix } = config;
    for (const name in config.deps) {
        includes += `-I ${config.includeDir}/${name} `;
        for (const libName of config.deps[name].libs) {
            const libPath = `${libDir}/${libPrefix}${libName}${libSuffix}`;
            libs += libPath + ' ';
        }
    }
    cxxflags += '-std=c++17 -m64 -march=native -mtune=native ';
    cxxflags += '-Wpedantic -Wall -Wextra ';
    cxxflags += '-Wno-unused-parameter -Wno-template-id-cdtor ';
    cxxflags += '-DV8_COMPRESS_POINTERS ';
    if (config.buildType === 'release') {
        cxxflags += '-O3 ';
    } else {
        cxxflags += '-g -no-pie -Og ';
    }
    ldflags += '-rdynamic -flto ';
    let content = '';
    content += `${targetPath}: ${objectPaths}\n`;
    content += `\tg++ ${ldflags} -Wl,--start-group ${libs} ${objectPaths} -Wl,--end-group `;
    content += `-o ${targetPath}\n`;
    if (config.buildType === 'release') {
        content += `\tstrip --strip-debug --strip-unneeded ${targetPath}\n`;
    }
    for (const sourcePath in projectObject) {
        const objectPath = projectObject[sourcePath].objectPath;
        let headerPaths = '';
        for (const headerPath of projectObject[sourcePath].headerPaths) {
            headerPaths += headerPath + ' ';
        }
        content += `\n${objectPath}: ${sourcePath} ${headerPaths}\n`;
        content += `\tg++ ${cxxflags} ${includes} -c ${sourcePath} -o ${objectPath}\n`;
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
            libs: [ 'v8' ]
        },
        openssl: {
            url: 'https://github.com/lmk97/build-openssl',
            version: '3.3.1',
            libs: [ 'ssl', 'crypto' ]
        },
        zlib: {
            url: 'https://github.com/lmk97/build-zlib',
            version: '1.3.1',
            libs: [ 'zlib' ]
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

await ungzipDeps(config);

if (config.platform === 'win32') {
    generateWinMakefile(config);
} else {
    generateUnixMakefile(config);
}

buildProject(config.platform);

})();
