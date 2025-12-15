API Reference: Performance Counters
===================================

This is the API reference for the functions, classes, and enums in the ``renderdoc`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: renderdoc

Counters
--------

.. autoclass:: renderdoc.CounterDescription
  :members:

.. autoclass:: renderdoc.CounterUnit
  :members:

.. autoclass:: renderdoc.Uuid
  :members:

Counter Types
-------------

.. autoclass:: renderdoc.GPUCounter
  :members:

.. autofunction:: renderdoc.IsAMDCounter
.. autofunction:: renderdoc.IsARMCounter
.. autofunction:: renderdoc.IsGenericCounter
.. autofunction:: renderdoc.IsIntelCounter
.. autofunction:: renderdoc.IsNvidiaCounter
.. autofunction:: renderdoc.IsVulkanExtendedCounter

Results
-------

.. autoclass:: renderdoc.CounterResult
  :members:

.. autoclass:: renderdoc.CounterValue
  :members:
