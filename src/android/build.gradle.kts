plugins {
    id("com.android.application") version "8.2.2"
}

android {
    namespace = "name.nick.jubanka.colleen"
    compileSdk = 34

    defaultConfig {
        applicationId = "name.nick.jubanka.colleen"
        minSdk = 21
        targetSdk = 34
        versionCode = 301
        versionName = "3.1"

        ndkVersion = "28.2.13676358"

        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                cFlags("-DANDROID", "-DDIRTYRECT", "-DSUPPORTS_PLATFORM_PALETTEUPDATE")
            }
        }
    }

    sourceSets {
        getByName("main") {
            manifest.srcFile("AndroidManifest.xml")
            java.srcDirs("src")
            res.srcDirs("res")
            jniLibs.srcDirs("libs")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    lint {
        abortOnError = false
    }
}
