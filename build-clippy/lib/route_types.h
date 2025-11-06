/* Auto-generated from route_types.txt by . */
/* Do not edit! */

#ifndef _FRR_ROUTE_TYPES_H
#define _FRR_ROUTE_TYPES_H

/* Zebra route's' types. */
#define ZEBRA_ROUTE_SYSTEM               0
#define ZEBRA_ROUTE_KERNEL               1
#define ZEBRA_ROUTE_CONNECT              2
#define ZEBRA_ROUTE_LOCAL                3
#define ZEBRA_ROUTE_STATIC               4
#define ZEBRA_ROUTE_RIP                  5
#define ZEBRA_ROUTE_RIPNG                6
#define ZEBRA_ROUTE_OSPF                 7
#define ZEBRA_ROUTE_OSPF6                8
#define ZEBRA_ROUTE_ISIS                 9
#define ZEBRA_ROUTE_BGP                  10
#define ZEBRA_ROUTE_PIM                  11
#define ZEBRA_ROUTE_EIGRP                12
#define ZEBRA_ROUTE_NHRP                 13
#define ZEBRA_ROUTE_HSLS                 14
#define ZEBRA_ROUTE_OLSR                 15
#define ZEBRA_ROUTE_TABLE                16
#define ZEBRA_ROUTE_LDP                  17
#define ZEBRA_ROUTE_VNC                  18
#define ZEBRA_ROUTE_VNC_DIRECT           19
#define ZEBRA_ROUTE_VNC_DIRECT_RH        20
#define ZEBRA_ROUTE_BGP_DIRECT           21
#define ZEBRA_ROUTE_BGP_DIRECT_EXT       22
#define ZEBRA_ROUTE_BABEL                23
#define ZEBRA_ROUTE_SHARP                24
#define ZEBRA_ROUTE_PBR                  25
#define ZEBRA_ROUTE_BFD                  26
#define ZEBRA_ROUTE_OPENFABRIC           27
#define ZEBRA_ROUTE_VRRP                 28
#define ZEBRA_ROUTE_NHG                  29
#define ZEBRA_ROUTE_SRTE                 30
#define ZEBRA_ROUTE_TABLE_DIRECT         31
#define ZEBRA_ROUTE_ALL                  32
#define ZEBRA_ROUTE_MAX                  33
#define ZEBRA_ROUTE_ERROR                255

#define SHOW_ROUTE_V4_HEADER \
  "Codes: K - kernel route, C - connected, L - local, S - static,\n" \
  "       R - RIP, O - OSPF, I - IS-IS, B - BGP, E - EIGRP, N - NHRP,\n" \
  "       T - Table, v - VNC, V - VNC-Direct, A - Babel, F - PBR,\n" \
  "       f - OpenFabric, t - Table-Direct,\n" \
  "       > - selected route, * - FIB route, q - queued, r - rejected, b - backup\n"  "       t - trapped, o - offload failure\n\n"
#define SHOW_ROUTE_V6_HEADER \
  "Codes: K - kernel route, C - connected, L - local, S - static,\n" \
  "       R - RIPng, O - OSPFv3, I - IS-IS, B - BGP, N - NHRP,\n" \
  "       T - Table, v - VNC, V - VNC-Direct, A - Babel, F - PBR,\n" \
  "       f - OpenFabric, t - Table-Direct,\n" \
  "       > - selected route, * - FIB route, q - queued, r - rejected, b - backup\n"  "       t - trapped, o - offload failure\n\n"

/* babeld */
#define FRR_REDIST_STR_BABELD \
  "<kernel|connected|local|static|rip|ripng|ospf|ospf6|isis|bgp|eigrp|nhrp|vnc|openfabric>"
#define FRR_REDIST_HELP_STR_BABELD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP_REDIST_STR_BABELD \
  "<kernel|connected|local|static|rip|ospf|isis|bgp|eigrp|nhrp|vnc|openfabric>"
#define FRR_IP_REDIST_HELP_STR_BABELD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP6_REDIST_STR_BABELD \
  "<kernel|connected|local|static|ripng|ospf6|isis|bgp|nhrp|vnc|openfabric>"
#define FRR_IP6_REDIST_HELP_STR_BABELD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "OpenFabric Routing Protocol\n"

/* bgpd */
#define FRR_REDIST_STR_BGPD \
  "<kernel|connected|local|static|rip|ripng|ospf|ospf6|isis|eigrp|nhrp|vnc|vnc-direct|babel|openfabric>"
#define FRR_REDIST_HELP_STR_BGPD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "VNC direct (not via zebra) routes\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP_REDIST_STR_BGPD \
  "<kernel|connected|local|static|rip|ospf|isis|eigrp|nhrp|vnc|vnc-direct|babel|openfabric>"
#define FRR_IP_REDIST_HELP_STR_BGPD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "VNC direct (not via zebra) routes\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP6_REDIST_STR_BGPD \
  "<kernel|connected|local|static|ripng|ospf6|isis|nhrp|vnc|vnc-direct|babel|openfabric>"
#define FRR_IP6_REDIST_HELP_STR_BGPD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "VNC direct (not via zebra) routes\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"

/* eigrpd */
#define FRR_REDIST_STR_EIGRPD \
  "<kernel|connected|local|static|rip|ospf|isis|bgp|nhrp|vnc|babel|openfabric>"
#define FRR_REDIST_HELP_STR_EIGRPD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"

/* fabricd */
#define FRR_REDIST_STR_FABRICD \
  "<kernel|connected|local|static|rip|ripng|ospf|ospf6|isis|bgp|eigrp|nhrp|vnc|babel>"
#define FRR_REDIST_HELP_STR_FABRICD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n"
#define FRR_IP_REDIST_STR_FABRICD \
  "<kernel|connected|local|static|rip|ospf|isis|bgp|eigrp|nhrp|vnc|babel>"
#define FRR_IP_REDIST_HELP_STR_FABRICD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n"
#define FRR_IP6_REDIST_STR_FABRICD \
  "<kernel|connected|local|static|ripng|ospf6|isis|bgp|nhrp|vnc|babel>"
#define FRR_IP6_REDIST_HELP_STR_FABRICD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n"

/* isisd */
#define FRR_REDIST_STR_ISISD \
  "<kernel|connected|local|static|rip|ripng|ospf|ospf6|bgp|eigrp|nhrp|vnc|babel|openfabric>"
#define FRR_REDIST_HELP_STR_ISISD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP_REDIST_STR_ISISD \
  "<kernel|connected|local|static|rip|ospf|bgp|eigrp|nhrp|vnc|babel|openfabric>"
#define FRR_IP_REDIST_HELP_STR_ISISD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP6_REDIST_STR_ISISD \
  "<kernel|connected|local|static|ripng|ospf6|bgp|nhrp|vnc|babel|openfabric>"
#define FRR_IP6_REDIST_HELP_STR_ISISD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"

/* nhrpd */
#define FRR_REDIST_STR_NHRPD \
  "<kernel|connected|local|static|rip|ripng|ospf|ospf6|isis|bgp|eigrp|vnc|babel|openfabric>"
#define FRR_REDIST_HELP_STR_NHRPD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP_REDIST_STR_NHRPD \
  "<kernel|connected|local|static|rip|ospf|isis|bgp|eigrp|vnc|babel|openfabric>"
#define FRR_IP_REDIST_HELP_STR_NHRPD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP6_REDIST_STR_NHRPD \
  "<kernel|connected|local|static|ripng|ospf6|isis|bgp|vnc|babel|openfabric>"
#define FRR_IP6_REDIST_HELP_STR_NHRPD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"

/* ospf6d */
#define FRR_REDIST_STR_OSPF6D \
  "<kernel|connected|local|static|ripng|isis|bgp|nhrp|vnc|babel|openfabric>"
#define FRR_REDIST_HELP_STR_OSPF6D \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"

/* ospfd */
#define FRR_REDIST_STR_OSPFD \
  "<kernel|connected|local|static|rip|isis|bgp|eigrp|nhrp|vnc|babel|openfabric>"
#define FRR_REDIST_HELP_STR_OSPFD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"

/* pbrd */
#define FRR_REDIST_STR_PBRD \
  "<kernel|connected|local|static|rip|ripng|ospf|ospf6|isis|bgp|eigrp|nhrp|vnc|babel|openfabric>"
#define FRR_REDIST_HELP_STR_PBRD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP_REDIST_STR_PBRD \
  "<kernel|connected|local|static|rip|ospf|isis|bgp|eigrp|nhrp|vnc|babel|openfabric>"
#define FRR_IP_REDIST_HELP_STR_PBRD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP6_REDIST_STR_PBRD \
  "<kernel|connected|local|static|ripng|ospf6|isis|bgp|nhrp|vnc|babel|openfabric>"
#define FRR_IP6_REDIST_HELP_STR_PBRD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"

/* ripd */
#define FRR_REDIST_STR_RIPD \
  "<kernel|connected|local|static|ospf|isis|bgp|eigrp|nhrp|vnc|babel|openfabric>"
#define FRR_REDIST_HELP_STR_RIPD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"

/* ripngd */
#define FRR_REDIST_STR_RIPNGD \
  "<kernel|connected|local|static|ospf6|isis|bgp|nhrp|vnc|babel|openfabric>"
#define FRR_REDIST_HELP_STR_RIPNGD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"

/* sharpd */
#define FRR_REDIST_STR_SHARPD \
  "<kernel|connected|local|static|rip|ripng|ospf|ospf6|isis|bgp|eigrp|nhrp|vnc|babel|openfabric>"
#define FRR_REDIST_HELP_STR_SHARPD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP_REDIST_STR_SHARPD \
  "<kernel|connected|local|static|rip|ospf|isis|bgp|eigrp|nhrp|vnc|babel|openfabric>"
#define FRR_IP_REDIST_HELP_STR_SHARPD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP6_REDIST_STR_SHARPD \
  "<kernel|connected|local|static|ripng|ospf6|isis|bgp|nhrp|vnc|babel|openfabric>"
#define FRR_IP6_REDIST_HELP_STR_SHARPD \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"

/* zebra */
#define FRR_REDIST_STR_ZEBRA \
  "<kernel|connected|local|static|rip|ripng|ospf|ospf6|isis|bgp|eigrp|nhrp|vnc|babel|openfabric>"
#define FRR_REDIST_HELP_STR_ZEBRA \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP_REDIST_STR_ZEBRA \
  "<kernel|connected|local|static|rip|ospf|isis|bgp|eigrp|nhrp|vnc|babel|openfabric>"
#define FRR_IP_REDIST_HELP_STR_ZEBRA \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP6_REDIST_STR_ZEBRA \
  "<kernel|connected|local|static|ripng|ospf6|isis|bgp|nhrp|vnc|babel|openfabric>"
#define FRR_IP6_REDIST_HELP_STR_ZEBRA \
  "Kernel routes (not installed via the zebra RIB)\n" \
  "Connected routes (directly attached subnet or host)\n" \
  "Local routes (directly attached host route)\n" \
  "Statically configured routes\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n"
#define FRR_IP_PROTOCOL_MAP_STR_ZEBRA \
  "<static|rip|ospf|isis|bgp|eigrp|nhrp|vnc|babel|openfabric|any>"
#define FRR_IP_PROTOCOL_MAP_HELP_STR_ZEBRA \
  "Statically configured routes\n" \
  "Routing Information Protocol (RIP)\n" \
  "Open Shortest Path First (OSPFv2)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Enhanced Interior Gateway Routing Protocol (EIGRP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n" \
  "Any of the above protocols\n"
#define FRR_IP6_PROTOCOL_MAP_STR_ZEBRA \
  "<static|ripng|ospf6|isis|bgp|nhrp|vnc|babel|openfabric|any>"
#define FRR_IP6_PROTOCOL_MAP_HELP_STR_ZEBRA \
  "Statically configured routes\n" \
  "Routing Information Protocol next-generation (IPv6) (RIPng)\n" \
  "Open Shortest Path First (IPv6) (OSPFv3)\n" \
  "Intermediate System to Intermediate System (IS-IS)\n" \
  "Border Gateway Protocol (BGP)\n" \
  "Next Hop Resolution Protocol (NHRP)\n" \
  "Virtual Network Control (VNC)\n" \
  "Babel routing protocol (Babel)\n" \
  "OpenFabric Routing Protocol\n" \
  "Any of the above protocols\n"


#ifdef FRR_DEFINE_DESC_TABLE

struct zebra_desc_table
{
  unsigned int type;
  const char *string;
  char chr;
};

#define DESC_ENTRY(T,S,C) [(T)] = { (T), (S), (C) }
static const struct zebra_desc_table route_types[] = {
  DESC_ENTRY	(ZEBRA_ROUTE_SYSTEM,	 "system",	'X' ),
  DESC_ENTRY	(ZEBRA_ROUTE_KERNEL,	 "kernel",	'K' ),
  DESC_ENTRY	(ZEBRA_ROUTE_CONNECT,	 "connected",	'C' ),
  DESC_ENTRY	(ZEBRA_ROUTE_LOCAL,	 "local",	'L' ),
  DESC_ENTRY	(ZEBRA_ROUTE_STATIC,	 "static",	'S' ),
  DESC_ENTRY	(ZEBRA_ROUTE_RIP,	 "rip",	'R' ),
  DESC_ENTRY	(ZEBRA_ROUTE_RIPNG,	 "ripng",	'R' ),
  DESC_ENTRY	(ZEBRA_ROUTE_OSPF,	 "ospf",	'O' ),
  DESC_ENTRY	(ZEBRA_ROUTE_OSPF6,	 "ospf6",	'O' ),
  DESC_ENTRY	(ZEBRA_ROUTE_ISIS,	 "isis",	'I' ),
  DESC_ENTRY	(ZEBRA_ROUTE_BGP,	 "bgp",	'B' ),
  DESC_ENTRY	(ZEBRA_ROUTE_PIM,	 "pim",	'P' ),
  DESC_ENTRY	(ZEBRA_ROUTE_EIGRP,	 "eigrp",	'E' ),
  DESC_ENTRY	(ZEBRA_ROUTE_NHRP,	 "nhrp",	'N' ),
  DESC_ENTRY	(ZEBRA_ROUTE_HSLS,	 "hsls",	'H' ),
  DESC_ENTRY	(ZEBRA_ROUTE_OLSR,	 "olsr",	'o' ),
  DESC_ENTRY	(ZEBRA_ROUTE_TABLE,	 "table",	'T' ),
  DESC_ENTRY	(ZEBRA_ROUTE_LDP,	 "ldp",	'L' ),
  DESC_ENTRY	(ZEBRA_ROUTE_VNC,	 "vnc",	'v' ),
  DESC_ENTRY	(ZEBRA_ROUTE_VNC_DIRECT,	 "vnc-direct",	'V' ),
  DESC_ENTRY	(ZEBRA_ROUTE_VNC_DIRECT_RH,	 "vnc-rn",	'V' ),
  DESC_ENTRY	(ZEBRA_ROUTE_BGP_DIRECT,	 "bgp-direct",	'b' ),
  DESC_ENTRY	(ZEBRA_ROUTE_BGP_DIRECT_EXT,	 "bgp-direct-to-nve-groups",	'e' ),
  DESC_ENTRY	(ZEBRA_ROUTE_BABEL,	 "babel",	'A' ),
  DESC_ENTRY	(ZEBRA_ROUTE_SHARP,	 "sharp",	'D' ),
  DESC_ENTRY	(ZEBRA_ROUTE_PBR,	 "pbr",	'F' ),
  DESC_ENTRY	(ZEBRA_ROUTE_BFD,	 "bfd",	'-' ),
  DESC_ENTRY	(ZEBRA_ROUTE_OPENFABRIC,	 "openfabric",	'f' ),
  DESC_ENTRY	(ZEBRA_ROUTE_VRRP,	 "vrrp",	'-' ),
  DESC_ENTRY	(ZEBRA_ROUTE_NHG,	 "zebra",	'-' ),
  DESC_ENTRY	(ZEBRA_ROUTE_SRTE,	 "srte",	'-' ),
  DESC_ENTRY	(ZEBRA_ROUTE_TABLE_DIRECT,	 "table-direct",	't' ),
  DESC_ENTRY	(ZEBRA_ROUTE_ALL,	 "any",	'-' ),
};
#undef DESC_ENTRY

#endif /* FRR_DEFINE_DESC_TABLE */

#endif /* _FRR_ROUTE_TYPES_H */
