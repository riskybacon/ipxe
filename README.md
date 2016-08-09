# iPXE for Emulab

As part of my work on diskless, iSCSI-on-root [Emulab](http://emulab.net), I need a bootloader that supports:

* The Emulab Bootinfo protocol
* The Emulab Event protocol
* Booting from an iSCSI device

The BTX-based Emulab bootloader is based on a FreeBSD 8, and getting an iSCSI boot enabled version of the Emulab bootloader sounded harder than putting the Emulab bootinfo and event subsystems into iPXE. 

Other advantages: 

* 60Kb PXE boot image (60Kb) versus the Emulab bootloader (187Kb). This allows us to burn this iPXE image onto our NIC's boot PROM.
* iPXE's scripting language. Customizing the PXE boot loader doesn't require a build environment or knowledge of C, which lowers the barrier for people that want to customize the PXE boot portion of Emulab.

So, I added those two things into iPXE. Here's the [fork](https://github.com/riskybacon/ipxe).

Quick start guide:

```
cd src
make
```

For any more detailed instructions, see [http://ipxe.org](http://ipxe.org)

# New commands

These new commands are available at the iPXE prompt or can be part
of an iPXE script

## bootinfo

Usage:

```
bootinfo <server name>
```

Gets the Emulab bootinfo for this computer.

Added iPXE settings

<table>
    <tr> <th> Setting </th> <th> Value </th> </tr>
    <tr> <td> bibootwhat_type_part </td> <td> 0x1 if bibootwhat_type_part is received </td> </tr>
    <tr> <td> bibootwhat_part      </td> <td> The partition to boot from </td> </tr>
    <tr> <td> bibootwhat_cmdline   </td> <td> The command line for bibootwhat_part </td> </tr>
    <tr> <td> bibootwhat_type_wait </td> <td> 0x1 if bibootwhat_type_wait is received </td> </tr>
    <tr> <td> bioboot_what_type_reboot </td> <td> 0x1 if bibootwhat_type_reboot is received </td> </tr>
    <tr> <td> bibootwhat_type_auto </td> <td> 0x1 if bibootwhat_type_auto is received </td> </tr>
    <tr> <td> bibootwhat_type_mfs  </td> <td> 0x1 if bibootwhat_type_mfs is received </td> </tr>
    <tr> <td> bibootwhat_mfs       </td> <td> TFTP Path to memory file system to boot </td> </tr>
</table>

For the values that are set to 0x1, use 

```
isset ${bibootwhat_type_*} || goto not_set
do stuff if set
```

In your iPXE script to test for the existence of that settting. I found that the double ampersand operator did not work how I expected and had better luck with the double pipe operator and goto.

## event

Sends an event from this node to the Emulab event system

```
event <dest server> <event name>
```

# Implementation 

The bootinfo portion of this iPXE fork used code from the Emulab code base. While iPXE uses very different mechanisms than FreeBSD to set up sockets and such, I was still able to use the bootinfo structs from the header files and the switch statement for inspecting the struct to fill the iPXE settings listed above. I was also able to lift the bootinfo print function almost verbatim. Win!

The event subsystem was created by using tcpdump to watch event packets on the wire and was re-implemented from scratch, which was much more straightforward than it sounds.

# Thanks

I would like to thank the [Flux Research Group](https://www.flux.utah.edu) at [The University of Utah](http://www.utah.edu) for their work on [Emulab](http://www.emulab.net). It's a great piece of software.

I would also like to thank all of the people that develop [iPXE](http://ipxe.org). The code is exceptionally easy to work with and is very high quality.

# Financial Support

This fork is funded by [The New Mexico Consortium](http://newmexicoconsortium.org) as part of the NSF funded [PRObE Project](http://nmc-probe.org)
