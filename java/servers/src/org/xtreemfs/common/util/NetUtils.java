/*
 * Copyright (c) 2008-2011 by Jan Stender,
 *               Zuse Institute Berlin
 *
 * Licensed under the BSD License, see LICENSE file for details.
 *
 */

package org.xtreemfs.common.util;

import java.io.IOException;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;

import org.xtreemfs.foundation.logging.Logging;
import org.xtreemfs.foundation.logging.Logging.Category;
import org.xtreemfs.pbrpc.generatedinterfaces.DIR.AddressMapping;

public class NetUtils {
    
    /**
     * Returns a list of mappings for all reachable network endpoints.
     * 
     * @param port
     *            the port to assign to the mappings
     * @param protocol
     *            the protocol for the endpoint
     * @param multihoming
     *            set to false if only the first reachable address should be returned
     * @return a list of mappings
     * @throws IOException
     */
    public static List<AddressMapping.Builder> getReachableEndpoints(int port, String protocol, boolean multihoming)
            throws IOException {
        
        List<AddressMapping.Builder> endpoints = new ArrayList<AddressMapping.Builder>(10);
        
        AddressMapping.Builder firstGlobal = null;
        AddressMapping.Builder firstLocal = null;
        
        // Try to find the global address by resolving the local hostname.
        String localHostAddress = null;
        try {
            localHostAddress = getHostAddress(InetAddress.getLocalHost());
        } catch (UnknownHostException e) {
            Logging.logMessage(Logging.LEVEL_WARN, Category.net, null, "Could not resolve the local hostnamme.",
                    new Object[0]);
        }

        // Iterate over the existing network interfaces and their addresses
        Enumeration<NetworkInterface> ifcs = NetworkInterface.getNetworkInterfaces();
        while (ifcs.hasMoreElements()) {
            NetworkInterface ifc = ifcs.nextElement();
            
            // Ignore loopback interfaces and interfaces that are down
            if (ifc.isLoopback() || !ifc.isUp())
                continue;
            
            List<InterfaceAddress> addrs = ifc.getInterfaceAddresses();
            for (InterfaceAddress addr : addrs) {
                InetAddress inetAddr = addr.getAddress();

                // Ignore local, wildcard and multicast addresses
                if (inetAddr.isLoopbackAddress() || inetAddr.isLinkLocalAddress() 
                        || inetAddr.isAnyLocalAddress() || inetAddr.isMulticastAddress())
                    continue;
                                               
                final String hostAddr = getHostAddress(inetAddr);
                final String uri = getURI(protocol, inetAddr, port);
                final String network = getNetworkCIDR(inetAddr, addr.getNetworkPrefixLength());
                AddressMapping.Builder amap = AddressMapping.newBuilder().setAddress(hostAddr).setPort(port)
                        .setMatchNetwork(network).setProtocol(protocol).setTtlS(3600).setUri(uri).setVersion(0)
                        .setUuid("");

                if (!multihoming) {
                    // If multihoming is disabled and this is the first local address, save it
                    if (inetAddr.isSiteLocalAddress() && firstLocal == null) {
                        firstLocal = amap;
                    }
                    
                    // If it is the first global address, save it, too and stop looking for other ones
                    if (!inetAddr.isSiteLocalAddress() && firstGlobal == null) {
                        firstGlobal = amap;
                    }

                    if (firstGlobal != null)
                        break;
                } else {
                    // For multihoming configurations add every endpoint 
                    endpoints.add(amap);
                }
            }
        }
        
        // add the first global address if one is found, or the first local otherwise
        if (!multihoming) {
            AddressMapping.Builder endpoint = (firstGlobal != null) ? firstGlobal : firstLocal;
            
            if (endpoint != null) {
                // Set the matching network to any
                endpoint.setMatchNetwork("*");
                endpoints.add(endpoint);
            }
        }

        // in case no IP address could be found at all, use 127.0.0.1 for local testing
        if (endpoints.isEmpty()) {
            Logging.logMessage(Logging.LEVEL_WARN, Category.net, null,
                "could not find a valid IP address, will use 127.0.0.1 instead", new Object[0]);
            AddressMapping.Builder amap = AddressMapping.newBuilder().setAddress("127.0.0.1").setPort(port)
                    .setProtocol(protocol).setTtlS(3600).setMatchNetwork("*")
                    .setUri(getURI(protocol, InetAddress.getLocalHost(), port)).setVersion(0).setUuid("");
            endpoints.add(amap);
        }
        
        return endpoints;
    }

    public static String getHostAddress(InetAddress host) {

        String hostAddr = host.getHostAddress();
        if (host instanceof Inet6Address) {
            if (hostAddr.lastIndexOf('%') >= 0) {
                hostAddr = hostAddr.substring(0, hostAddr.lastIndexOf('%'));
            }
        }

        return hostAddr;

    }

    public static String getURI(String protocol, InetAddress host, int port) {

        String hostAddr = host.getHostAddress();
        if (host instanceof Inet6Address) {
            if (hostAddr.lastIndexOf('%') >= 0) {
                hostAddr = hostAddr.substring(0, hostAddr.lastIndexOf('%'));
            }
            hostAddr = "["+hostAddr+"]";
        }

        return protocol+"://"+hostAddr+":"+port;

    }
    
    public static String getDomain(String hostName) {
        int i = hostName.indexOf('.');
        return i == -1? "": hostName.substring(i + 1);
    }
    
    private static String getSubnetMaskString(short prefixLength) {
        
        long addr = (0xFFFFFFFFL << (32 - prefixLength)) & 0xFFFFFFFFL;
        StringBuffer sb = new StringBuffer();
        for (int i = 3; i >= 0; i--) {
            sb.append((addr & (0xFF << (i * 8))) >> (i * 8));
            if (i > 0)
                sb.append(".");
        }
        
        return sb.toString();
    }
    
    private static String getNetworkCIDR(InetAddress addr, short prefixLength) {
        byte[] raw = addr.getAddress();

        // If the address is more then 32bit long it has to be an v6 address.
        boolean isV6 = raw.length > 4;

        // Get the number of fields that are network specific and null the remaining host fields.
        int networkFields = prefixLength / 8;
        for (int i = networkFields + 1; i < raw.length; i++) {
            raw[i] = 0x00;
        }

        // Get the remaining bytes attributed to the network amidst a byte field.
        int networkRemainder = prefixLength % 8;
        if (networkFields < raw.length) {
            // Construct a 8bit mask, with bytes set for the network.
            byte mask = (byte) (0xFF << (8 - networkRemainder));
            raw[networkFields] = (byte) (raw[networkFields] & mask);
        }

        StringBuilder sb = new StringBuilder();

        // Use the InetAddress implementation to convert the raw byte[] to a string.
        try {
            sb.append(InetAddress.getByAddress(raw).getHostAddress());
        } catch (UnknownHostException e) {
            // This should never happen, since the foundation of every calculation is the byte array
            // returned by a valid InetAddress
            throw new RuntimeException(e);
        }

        sb.append("/").append(prefixLength);

        return sb.toString();
    }

    public static void main(String[] args) throws Exception {
        
        System.out.println("all network interfaces: ");
        Enumeration<NetworkInterface> ifcs = NetworkInterface.getNetworkInterfaces();
        while (ifcs.hasMoreElements()) {
            for (InterfaceAddress addr : ifcs.nextElement().getInterfaceAddresses()) {
                InetAddress inetAddr = addr.getAddress();
                System.out.println(inetAddr + ", loopback: " + inetAddr.isLoopbackAddress() + ", linklocal: "
                    + inetAddr.isLinkLocalAddress() + ", reachable: " + inetAddr.isReachable(1000));
            }
        }
        
        System.out.println("\nsuitable network interfaces: ");
        for (AddressMapping.Builder endpoint : NetUtils.getReachableEndpoints(32640, "http", true))
            System.out.println(endpoint.build().toString());
    }
    
}
