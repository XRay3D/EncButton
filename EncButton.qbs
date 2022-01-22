import qbs
import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.Probes


/*
    avrdude
    -c arduino -p m328p -P COMХХХ -b 115200 -U flash:w:%{CurrentBuild:QbsBuildRoot}%{ActiveProject:Name}.hex:i
*/

CppApplication {
    // property string CORE: 'C:/Users/X-Ray/AppData/Local/Arduino15/packages/MightyCore/hardware/avr/2.1.3/cores/MCUdude_corefiles'
    // property string VARIANT: 'C:/Users/X-Ray/AppData/Local/Arduino15/packages/MightyCore/hardware/avr/2.1.3/variants/standard'

    property string USER: 'X-Ray'
    property string CORE: 'C:/Users/'+USER+'/AppData/Local/Arduino15/packages/MightyCore/hardware/avr/2.1.3/cores/MCUdude_corefiles'
    property string VARIANT: 'C:/Users/'+USER+'/AppData/Local/Arduino15/packages/MightyCore/hardware/avr/2.1.3/variants/standard'
    property string ARDUINO_LIB: 'Ваша директория установки библиотек Arduino'
    property string MCU: 'atmega32'

    type: [
        // 'application',
        'hex',
    ]

    // Depends { name:'cpp' }

    Group {
        name: 'App (Sketch)'
        prefix: path
        files: [
            '/**/*.c',
            '/**/*.cpp',
            '/**/*.h',
            //'/**/*.ino',
        ]
        excludeFiles: [
            '/TestApp/**',
            '/build*/**',
        ]
    }


    Group {
        name: 'Arduino Core'
        prefix: CORE
        files: [
            '/WInterrupts.c',
            '/hooks.c',
            '/wiring.c',
            '/wiring_analog.c',
            '/wiring_digital.c',
            '/wiring_pulse.c',
            '/wiring_shift.c',
            '/CDC.cpp',
            '/HardwareSerial.cpp',
            '/HardwareSerial0.cpp',
            '/HardwareSerial1.cpp',
            '/HardwareSerial2.cpp',
            '/HardwareSerial3.cpp',
            '/IPAddress.cpp',
            '/PluggableUSB.cpp',
            '/Print.cpp',
            '/Stream.cpp',
            '/Tone.cpp',
            '/USBCore.cpp',
            '/WMath.cpp',
            '/WString.cpp',
            '/abi.cpp',
            '/main.cpp',
            '/new.cpp',
            '/wiring_extras.cpp',
            '/wiring_pulse.S',

        ]
        excludeFiles: [
        ]
    }

    // Group {
    // id:libraries
    // name: 'Arduino Libraries'
    // prefix: path+'/../EncButton/'
    // files: [
    // '/**/*.c',
    // '/**/*.cpp',
    // '/**/*.h',
    // '/**/*.ino',
    // ]
    // }

    cpp.includePaths:{
        var includePaths = [
                    CORE,
                    VARIANT,
                    path,
                ]

        var exclude = []
        var func = function(incPath){
            var filePpaths = File.directoryEntries(incPath, File.Files)
            var size = includePaths.length
            var fl = true
            for(var i in exclude)
                if(incPath.endsWith(exclude[i]))
                    fl = false
            for(var i in filePpaths)
                if(fl && size === includePaths.length && (filePpaths[i].endsWith('.h')||filePpaths[i].endsWith('.hpp')))
                    includePaths.push(incPath)
            var dirPaths = File.directoryEntries(incPath, File.Dirs | File.NoDotAndDotDot)
            for(var i in dirPaths)
                func(FileInfo.joinPaths(incPath, dirPaths[i]))
        }
        func(path)
        return includePaths
    }



    consoleApplication: true

    cpp.debugInformation: false
    cpp.enableExceptions: false
    cpp.enableRtti: false

    cpp.executableSuffix: '.elf'
    cpp.optimization: 'small' // passes -Os
    cpp.positionIndependentCode: false
    //cpp.warningLevel: 'none' // passes -w
    cpp.warningLevel: 'all'

    cpp.cLanguageVersion: 'c11'
    cpp.cxxLanguageVersion: 'c++11'

    cpp.defines: [
        'F_CPU=12000000L',
        'ARDUINO=10815',
        'ARDUINO_AVR_ATmega32',
        'ARDUINO_ARCH_AVR',
    ]

    cpp.cxxFlags: [
        '-c',
        '-g',
        '-Os',
        '-w',
        '-fno-threadsafe-statics',
    ]

    cpp.assemblerFlags:[
        '-c',
        '-g',
        '-x', 'assembler-with-cpp',
        '-mmcu='+MCU,
    ]

    cpp.cFlags: [
        '-c',
        '-g',
        '-Os',
        '-w',
    ]

    cpp.commonCompilerFlags: [
        '-mmcu='+MCU,
        '-MMD',
        '-fdata-sections',
        '-ffunction-sections',
        '-Werror',
        '-Wextra',
        '-Wall',
    ]

    cpp.driverLinkerFlags:[
        '-mmcu='+MCU,
        '-Os',
        '-w',
        '-flto',
    ]

    cpp.linkerFlags:[
        '--gc-sections',
        '--section-start=.FAR_MEM1=0x10000',
        '-flto',
        '-lm',
    ]

    Rule {
        inputs: ['application']

        Artifact
        {
            filePath: project.buildDirectory + product.name + '.hex'
            fileTags: 'hex'
        }

        prepare:
        {
            var argsSize = [
                        // '-A',
                        input.filePath,
                        '--mcu='+product.MCU,
                        '-C',
                        '--format=avr'
                    ]
            var argsObjcopyHex = [
                        '-O', 'ihex',
                        '-j', '.text',
                        '-j', '.data',
                        input.filePath,
                        output.filePath
                    ]
            var argsObjcopyBin = [
                        '-O', 'binary',
                        '-S',
                        '-g',
                        input.filePath,
                        project.buildDirectory + product.name + '.bin'
                    ]
            var argsFlashing = [
                        '-c', 'arduino',
                        '-p', 'm32',
                        '-P', 'COM5',
                        '-b', '115200',
                        '-U', 'flash:w:'+output.filePath+':i'
                    ]

            var cmdSize = new Command( 'avr-size.exe', argsSize)
            var cmdObjcopyHex = new Command( 'avr-objcopy.exe', argsObjcopyHex)
            var cmdObjcopyBin = new Command( 'avr-objcopy.exe', argsObjcopyBin)

            /*
             Если не работает то скопировать 'avrdude.conf' из
             C:/Program Files (x86)/Arduino/hardware/arduino/avr/bootloaders/gemma
             в
             C:/Program Files (x86)/Arduino/hardware/tools/avr/bin
             */
            var cmdFlash = new Command( 'avrdude.exe', argsFlashing);

            cmdSize.description = 'Size of sections:'
            cmdSize.highlight = 'filegen'

            cmdObjcopyHex.description = 'convert to hex...'
            cmdObjcopyHex.highlight = 'filegen'

            cmdObjcopyBin.description = 'convert to bin...'
            cmdObjcopyBin.highlight = 'filegen'

            cmdFlash.description = 'download firmware to uC...'
            cmdFlash.highlight = 'filegen'
            return [
                        cmdSize,
                        cmdObjcopyHex,
                        cmdObjcopyBin,
                        // cmdFlash // Закомметировать если не нужно шить
                    ]
        }
    }
}

//Linking everything together...
//'C:/Users/X-Ray/AppData/Local/Arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin/avr-gcc'
// -w
// -Os
// -Wl,--gc-sections,--section-start=.FAR_MEM1=0x10000
// -mmcu=atmega32
// -o 'C:/Users/X-Ray/AppData/Local/Temp/arduino_build_318048/sketch_jan16b.ino.elf' 'C:/Users/X-Ray/AppData/Local/Temp/arduino_build_318048/sketch/sketch_jan16b.ino.cpp.o' 'C:/Users/X-Ray/AppData/Local/Temp/arduino_build_318048/core/core.a'
// '-LC:/Users/X-Ray/AppData/Local/Temp/arduino_build_318048'
// -lm
//'C:/Users/X-Ray/AppData/Local/Arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin/avr-objcopy'
// -O ihex
// -j .eeprom
// --set-section-flags=.eeprom=alloc,load
// --no-change-warnings
// --change-section-lma .eeprom=0 'C:/Users/X-Ray/AppData/Local/Temp/arduino_build_318048/sketch_jan16b.ino.elf' 'C:/Users/X-Ray/AppData/Local/Temp/arduino_build_318048/sketch_jan16b.ino.eep'
//'C:/Users/X-Ray/AppData/Local/Arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin/avr-objcopy'
// -O ihex
// -R .eeprom 'C:/Users/X-Ray/AppData/Local/Temp/arduino_build_318048/sketch_jan16b.ino.elf' 'C:/Users/X-Ray/AppData/Local/Temp/arduino_build_318048/sketch_jan16b.ino.hex'
//cmd /C 'C:/Users/X-Ray/AppData/Local/Arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin/avr-objdump'
// --disassemble
// --source
// --line-numbers
// --demangle
// --section=.text 'C:/Users/X-Ray/AppData/Local/Temp/arduino_build_318048/sketch_jan16b.ino.elf' > 'C:/Users/X-Ray/AppData/Local/Temp/arduino_build_318048/sketch_jan16b.ino_atmega32_12000000L.lst'
//'C:/Users/X-Ray/AppData/Local/Arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin/avr-size'
// -A 'C:/Users/X-Ray/AppData/Local/Temp/arduino_build_318048/sketch_jan16b.ino.elf'
//Скетч использует 352 байт (1%) памяти устройства. Всего доступно 32256 байт.
//Глобальные переменные используют 6 байт (0%) динамической памяти, оставляя 2042 байт для локальных переменных. Максимум: 2048 байт.


/*


bool : allowUnresolvedSymbols
bool : alwaysUseLipo
bool : automaticReferenceCounting
bool : combineCSources
bool : combineCxxSources
bool : combineObjcSources
bool : combineObjcxxSources
bool : createSymlinks
bool : debugInformation
bool : discardUnusedData
bool : enableExceptions
bool : enableReproducibleBuilds
bool : enableRtti
bool : enableSuspiciousLinkerFlagWarnings
bool : generateAssemblerListingFiles
bool : generateCompilerListingFiles
bool : generateLinkerMapFile
bool : generateManifestFile
bool : positionIndependentCode
bool : removeDuplicateLibraries
bool : requireAppContainer
bool : requireAppExtensionSafeApi
bool : separateDebugInformation
bool : treatSystemHeadersAsDependencies
bool : treatWarningsAsErrors
bool : useCPrecompiledHeader
bool : useCxxPrecompiledHeader
bool : useLanguageVersionFallback
bool : useObjcPrecompiledHeader
bool : useObjcxxPrecompiledHeader
bool : useRPathLink
bool : useRPaths
int : compilerVersionMajor
int : compilerVersionMinor
int : compilerVersionPatch
pathList : compilerFrameworkPaths
pathList : compilerIncludePaths
pathList : compilerLibraryPaths
pathList : distributionFrameworkPaths
pathList : distributionIncludePaths
pathList : distributionLibraryPaths
pathList : frameworkPaths
pathList : includePaths
pathList : libraryPaths
pathList : prefixHeaders
pathList : systemFrameworkPaths
pathList : systemIncludePaths
string : architecture
string : archiverName
string : archiverPath
string : assemblerListingSuffix
string : assemblerName
string : assemblerPath
string : compilerListingSuffix
string : compilerName
string : compilerPath
string : compilerVersion
string : cxxStandardLibrary
string : debugInfoBundleSuffix
string : debugInfoSuffix
string : dsymutilPath
string : dynamicLibraryImportSuffix
string : dynamicLibraryPrefix
string : dynamicLibrarySuffix
string : enableCxxLanguageMacro
string : endianness
string : entryPoint
string : exceptionHandlingModel
string : executablePrefix
string : executableSuffix
string : exportedSymbolsCheckMode
string : linkerMapSuffix
string : linkerMode
string : linkerName
string : linkerPath
string : linkerVariant
string : lipoPath
string : loadableModulePrefix
string : loadableModuleSuffix
string : minimumIosVersion
string : minimumMacosVersion
string : minimumTvosVersion
string : minimumWatchosVersion
string : minimumWindowsVersion
string : nmName
string : nmPath
string : objcopyName
string : objcopyPath
string : objectSuffix
string : optimization
string : rpathLinkFlag
string : rpathOrigin
string : runtimeLibrary
string : soVersion
string : sonamePrefix
string : staticLibraryPrefix
string : staticLibrarySuffix
string : stripName
string : stripPath
string : toolchainInstallPath
string : variantSuffix
string : visibility
string : warningLevel
string : windowsApiCharacterSet
string : windowsApiFamily
string : windowsSdkVersion
stringList : assemblerFlags
stringList : cFlags
stringList : cLanguageVersion
stringList : commonCompilerFlags
stringList : compilerWrapper
stringList : cppFlags
stringList : cxxFlags
stringList : cxxLanguageVersion
stringList : defines
stringList : driverFlags
stringList : driverLinkerFlags
stringList : dsymutilFlags
stringList : dynamicLibraries
stringList : enableCompilerDefinesByLanguage
stringList : frameworks
stringList : linkerFlags
stringList : linkerWrapper
stringList : objcFlags
stringList : objcxxFlags
stringList : platformDefines
stringList : rpaths
stringList : staticLibraries
stringList : systemRunPaths
stringList : weakFrameworks
stringList : windowsApiAdditionalPartitions
var : compilerDefinesByLanguage
var : compilerPathByLanguage



*/
