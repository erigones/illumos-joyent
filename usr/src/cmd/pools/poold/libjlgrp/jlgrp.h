/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_sun_solaris_service_locality_LocalityDomain */

#ifndef _Included_com_sun_solaris_service_locality_LocalityDomain
#define _Included_com_sun_solaris_service_locality_LocalityDomain
#ifdef __cplusplus
extern "C" {
#endif
#undef com_sun_solaris_service_locality_LocalityDomain_LGRP_VIEW_CALLER
#define com_sun_solaris_service_locality_LocalityDomain_LGRP_VIEW_CALLER 0L
#undef com_sun_solaris_service_locality_LocalityDomain_LGRP_VIEW_OS
#define com_sun_solaris_service_locality_LocalityDomain_LGRP_VIEW_OS 1L
/*
 * Class:     com_sun_solaris_service_locality_LocalityDomain
 * Method:    jl_init
 * Signature: (I)J
 */
JNIEXPORT jlong JNICALL Java_com_sun_solaris_service_locality_LocalityDomain_jl_1init
  (JNIEnv *, jobject, jint);

/*
 * Class:     com_sun_solaris_service_locality_LocalityDomain
 * Method:    jl_fini
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_sun_solaris_service_locality_LocalityDomain_jl_1fini
  (JNIEnv *, jobject);

/*
 * Class:     com_sun_solaris_service_locality_LocalityDomain
 * Method:    jl_root
 * Signature: ()Lcom/sun/solaris/service/locality/LocalityGroup;
 */
JNIEXPORT jobject JNICALL Java_com_sun_solaris_service_locality_LocalityDomain_jl_1root
  (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif
/* Header for class com_sun_solaris_service_locality_LocalityGroup */

#ifndef _Included_com_sun_solaris_service_locality_LocalityGroup
#define _Included_com_sun_solaris_service_locality_LocalityGroup
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_sun_solaris_service_locality_LocalityGroup
 * Method:    jl_children
 * Signature: ()[J
 */
JNIEXPORT jlongArray JNICALL Java_com_sun_solaris_service_locality_LocalityGroup_jl_1children
  (JNIEnv *, jobject);

/*
 * Class:     com_sun_solaris_service_locality_LocalityGroup
 * Method:    jl_cpus
 * Signature: ()[I
 */
JNIEXPORT jintArray JNICALL Java_com_sun_solaris_service_locality_LocalityGroup_jl_1cpus
  (JNIEnv *, jobject);

/*
 * Class:     com_sun_solaris_service_locality_LocalityGroup
 * Method:    jl_latency
 * Signature: (JJ)I
 */
JNIEXPORT jint JNICALL Java_com_sun_solaris_service_locality_LocalityGroup_jl_1latency
  (JNIEnv *, jobject, jlong, jlong);

#ifdef __cplusplus
}
#endif
#endif
