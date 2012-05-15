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

   The security namespace, used by kernel security modules.

.. data:: NS_SYSTEM

   The system namespace, used by the kernel to store things such as
   ACLs and capabilities.

.. data:: NS_TRUSTED

   The trusted namespace, visible and accessibly only to trusted
   processes, used to implement mechanisms in user space.

.. data:: NS_USER

   The user namespace; this is the namespace accessible to
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
