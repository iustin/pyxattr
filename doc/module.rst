Interface to extended filesystem attributes
===========================================

.. automodule:: xattr

Constants
---------

.. data:: XATTR_CREATE

   Used as flags value, the target attribute
   will be created, giving an error if it already exists.

.. data:: XATTR_REPLACE

   Used as flags value, the target attribute
   will be replaced, giving an error if it doesn't exist.

.. data:: NS_SECURITY

   The security name space, used by kernel security modules to store
   (for example) capabilities information.

.. data:: NS_SYSTEM

   The system name space, used by the kernel to store (for example)
   ACLs.

.. data:: NS_TRUSTED

   The trusted name space, visible and accessibly only to trusted
   processes, used to implement mechanisms in user space.

.. data:: NS_USER

   The user name space; this is the name space accessible to
   non-privileged processes.

Functions
---------

.. autofunction:: list
.. autofunction:: get
.. autofunction:: get_all
.. autofunction:: set
.. autofunction:: remove


Deprecated functions
--------------------

.. autofunction:: getxattr
.. autofunction:: setxattr
.. autofunction:: listxattr
.. autofunction:: removexattr
