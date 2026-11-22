#!/bin/sh
# Baja las librerias cerradas de la GPU (PowerVR SGX544, DDK 1.12@3052601) del
# Android publico de la LeMaker Guitar. No van en este repo: son de Imagination.
set -e
cd "$(dirname "$0")"
B=https://raw.githubusercontent.com/LeMaker/android-actions/master/android/device/actions/common/prebuilt/gpu/atm7059
for f in vendor/bin/pvrsrvctl \
  vendor/lib/egl/libEGL_POWERVR_SGX544_115.so vendor/lib/egl/libGLESv1_CM_POWERVR_SGX544_115.so \
  vendor/lib/egl/libGLESv2_POWERVR_SGX544_115.so vendor/lib/hw/gralloc.S500.so \
  vendor/lib/hw/hwcomposer.S500.so vendor/lib/hw/memtrack.S500.so vendor/lib/libIMGegl.so \
  vendor/lib/libPVRScopeServices.so vendor/lib/libglslcompiler.so vendor/lib/libpvr2d.so \
  vendor/lib/libpvrANDROID_WSEGL.so vendor/lib/libsrv_init.so vendor/lib/libsrv_um.so vendor/lib/libusc.so; do
    mkdir -p "blobs-guitar/$(dirname $f)"
    curl -sfL -o "blobs-guitar/$f" "$B/$f"
done
strings blobs-guitar/vendor/lib/egl/libGLESv2_POWERVR_SGX544_115.so | grep -m1 "build 1.12@"
