// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
// Author: Ryan Pavlik <ryan.pavlik@collabora.com>

#include "android_load_class.h"

#include "util/u_logging.h"

#include "wrap/android.content.h"
#include "wrap/dalvik.system.h"

#include "jni.h"

using wrap::android::content::Context;
using wrap::android::content::pm::ApplicationInfo;
using wrap::android::content::pm::PackageManager;

static ApplicationInfo
getAppInfo(std::string const &packageName, jobject application_context)
{
	try {
		auto context = Context{application_context};
		auto packageManager =
		    PackageManager{context.getPackageManager()};
		auto packageInfo = packageManager.getPackageInfo(
		    packageName, PackageManager::GET_META_DATA |
		                     PackageManager::GET_SHARED_LIBRARY_FILES);
		return packageInfo.getApplicationInfo();
	} catch (std::exception const &e) {
		return {};
	}
}

static wrap::java::lang::Class
loadClassFromPackage(ApplicationInfo applicationInfo,
                     jobject application_context,
                     const char *clazz_name)
{
	auto context = Context{application_context}.getApplicationContext();
	auto pkgContext = context.createPackageContext(
	    applicationInfo.getPackageName(),
	    Context::CONTEXT_IGNORE_SECURITY | Context::CONTEXT_INCLUDE_CODE);

	// Not using ClassLoader.loadClass because it expects a /-delimited
	// class name, while we have a .-delimited class name.
	// This does work
	wrap::java::lang::ClassLoader pkgClassLoader =
	    pkgContext.getClassLoader();

	auto loadedClass = wrap::java::lang::Class::forName(
	    clazz_name, true, pkgClassLoader.object());
	return loadedClass;
}

void *
android_load_class_from_package(struct _JavaVM *vm,
                                const char *pkgname,
                                void *application_context,
                                const char *classname)
{
	jni::init(vm);
	Context context((jobject)application_context);
	auto info = getAppInfo(pkgname, (jobject)application_context);
	if (info.isNull()) {
		U_LOG_E("Could not get application info for package '%s'",
		        pkgname);
		return nullptr;
	}
	auto clazz =
	    loadClassFromPackage(info, (jobject)application_context, classname);
	if (clazz.isNull()) {
		U_LOG_E("Could not load class '%s' from package '%s'",
		        classname, pkgname);
		return nullptr;
	}
	return clazz.object().getHandle();
}