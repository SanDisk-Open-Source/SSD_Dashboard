
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __java_net_URLClassLoader__
#define __java_net_URLClassLoader__

#pragma interface

#include <java/security/SecureClassLoader.h>
#include <gcj/array.h>

extern "Java"
{
  namespace gnu
  {
    namespace java
    {
      namespace net
      {
        namespace loader
        {
            class Resource;
            class URLStreamHandlerCache;
        }
      }
    }
  }
  namespace java
  {
    namespace net
    {
        class URL;
        class URLClassLoader;
        class URLStreamHandlerFactory;
    }
    namespace security
    {
        class AccessControlContext;
        class CodeSource;
        class PermissionCollection;
    }
  }
}

class java::net::URLClassLoader : public ::java::security::SecureClassLoader
{

public:
  URLClassLoader(JArray< ::java::net::URL * > *);
  URLClassLoader(JArray< ::java::net::URL * > *, ::java::lang::ClassLoader *);
public: // actually package-private
  URLClassLoader(::java::lang::ClassLoader *, ::java::security::AccessControlContext *);
public:
  URLClassLoader(JArray< ::java::net::URL * > *, ::java::lang::ClassLoader *, ::java::net::URLStreamHandlerFactory *);
public: // actually protected
  virtual void addURL(::java::net::URL *);
private:
  void addURLImpl(::java::net::URL *);
  void addURLs(JArray< ::java::net::URL * > *);
  ::java::lang::String * getAttributeValue(::java::util::jar::Attributes$Name *, ::java::util::jar::Attributes *, ::java::util::jar::Attributes *);
public: // actually protected
  virtual ::java::lang::Package * definePackage(::java::lang::String *, ::java::util::jar::Manifest *, ::java::net::URL *);
  virtual ::java::lang::Class * findClass(::java::lang::String *);
public:
  virtual ::java::lang::String * toString();
private:
  ::gnu::java::net::loader::Resource * findURLResource(::java::lang::String *);
public:
  virtual ::java::net::URL * findResource(::java::lang::String *);
  virtual ::java::util::Enumeration * findResources(::java::lang::String *);
public: // actually protected
  virtual ::java::security::PermissionCollection * getPermissions(::java::security::CodeSource *);
public:
  virtual JArray< ::java::net::URL * > * getURLs();
  static ::java::net::URLClassLoader * newInstance(JArray< ::java::net::URL * > *);
  static ::java::net::URLClassLoader * newInstance(JArray< ::java::net::URL * > *, ::java::lang::ClassLoader *);
public: // actually package-private
  static ::java::lang::Class * access$0(::java::net::URLClassLoader *, ::java::lang::String *, JArray< jbyte > *, jint, jint, ::java::security::CodeSource *);
private:
  static ::gnu::java::net::loader::URLStreamHandlerCache * factoryCache;
  static ::java::lang::String * URL_LOADER_PREFIX;
  ::java::util::Vector * __attribute__((aligned(__alignof__( ::java::security::SecureClassLoader)))) urls;
  ::java::util::Vector * urlinfos;
  ::java::net::URLStreamHandlerFactory * factory;
  ::java::security::AccessControlContext * securityContext;
  ::java::lang::String * thisString;
public:
  static ::java::lang::Class class$;
};

#endif // __java_net_URLClassLoader__