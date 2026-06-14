cmake_minimum_required(VERSION 3.18)
project(atari800 C)

set(A800_CORE_SRCS
    ../afile.c
    ../antic.c
    ../artifact.c
    ../atari.c
    ../binload.c
    ../cartridge.c
    ../cartridge_info.c
    ../cassette.c
    ../cfg.c
    ../colours.c
    ../colours_external.c
    ../colours_pal.c
    ../colours_ntsc.c
    ../compfile.c
    ../cpu.c
    ../crc32.c
    ../cycle_map.c
    ../devices.c
    ../esc.c
    ../gtia.c
    ../img_tape.c
    ../input.c
    ../log.c
    ../memory.c
    ../monitor.c
    ../mzpokeysnd.c
    ../pbi.c
    ../pbi_bb.c
    ../pbi_mio.c
    ../pbi_scsi.c
    ../pbi_xld.c
    ../pia.c
    ../pokey.c
    ../pokeyrec.c
    ../pokeysnd.c
    ../remez.c
    ../roms/altirra_5200_os.c
    ../roms/altirra_5200_charset.c
    ../roms/altirra_basic.c
    ../roms/altirraos_800.c
    ../roms/altirraos_xl.c
    ../rtime.c
    ../screen.c
    ../sio.c
    ../statesav.c
    ../sysrom.c
    ../ui.c
    ../ui_basic.c
    ../util.c
    ../file_export.c
    ../netsio.c
    ../rdevice.c
    ../voicebox.c
    ../votrax.c
    ../votraxsnd.c
    ../codecs/image.c
    ../codecs/image_pcx.c
    ../codecs/container.c
    ../codecs/container_wav.c
    ../codecs/audio.c
    ../codecs/audio_pcm.c
    ../codecs/audio_adpcm.c
    ../codecs/audio_mulaw.c
    ../codecs/container_avi.c
    ../codecs/video.c
    ../codecs/video_mrle.c
    ../codecs/video_zmbv.c
)

set(ANDROID_JNI_SRCS
    jni/androidinput.c
    jni/graphics.c
    jni/jni.c
    jni/platform.c
    jni/sound.c
)

add_library(atari800 SHARED ${A800_CORE_SRCS} ${ANDROID_JNI_SRCS})

target_include_directories(atari800 PRIVATE
    ..
)

target_link_libraries(atari800
    log
    GLESv1_CM
    z
    m
)

set_target_properties(atari800 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
