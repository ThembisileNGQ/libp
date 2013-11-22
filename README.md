Least Interferance Beaconing Protocol
=====================================

The Least Interferance Beaconing Protocol (LIBP) is a routing protocol designed for Low Powered and Lossy Networks (LLNs).

This routing protocol, like CTP, uses a beaconing process initiated by the source (sink) node. When 
the process is initiated nodes incident to the sink node will be the first to recognize that
a sink node is within one hop distance. This process is then initiated by these nodes to their
neighbors and this process is repeated thereafter. This results in a network where each node is aware of its
neighbors. The least interference paradigm is integrated into the process by which nodes select
parent nodes which have the smallest number of (supporting) children, which is the parent of least
traffic flow interference. This configuration is especially powerful in the situation where sensors
are periodically sensing information (which is a very popular sensor use case).
LIBP basically aims to provide a way to balance traffic flow in such a way that it results in
energy efficiency by having a network where nodes support less traffic. 

Research
========

This implementation of LIBP is intended for research purposes. This implementation of LIBP has been
compared against two other popular wireless sensor network routing protocol, that being RPL and CTP. further
results will be shown here after the work gets published.

This LIBP implementation has been built for ContikiOS. To implement this LIBP, the CTP code from Contiki was forked and
modified in order to conform to the LIBP paradigm. LIBP runs ontop of Rime. The lightweight communications protocol of Contiki.

Contiki
=======

Contiki is an open source operating system that runs on tiny low-power
microcontrollers and makes it possible to develop applications that
make efficient use of the hardware while providing standardized
low-power wireless communication for a range of hardware platforms.

For more information, see the Contiki website:

[http://contiki-os.org](http://contiki-os.org)
