/*
 * Copyright (c) 2012-2013 by Johannes Dillmann,
 *               Zuse Institute Berlin
 *
 * Licensed under the BSD License, see LICENSE file for details.
 *
 */

package org.xtreemfs.osd.operations;

import org.xtreemfs.common.Capability;
import org.xtreemfs.common.ReplicaUpdatePolicies;
import org.xtreemfs.common.uuids.ServiceUUID;
import org.xtreemfs.common.xloc.InvalidXLocationsException;
import org.xtreemfs.common.xloc.XLocations;
import org.xtreemfs.foundation.pbrpc.generatedinterfaces.RPC.ErrorType;
import org.xtreemfs.foundation.pbrpc.generatedinterfaces.RPC.POSIXErrno;
import org.xtreemfs.foundation.pbrpc.generatedinterfaces.RPC.RPCHeader.ErrorResponse;
import org.xtreemfs.foundation.pbrpc.utils.ErrorUtils;
import org.xtreemfs.osd.OSDRequest;
import org.xtreemfs.osd.OSDRequestDispatcher;
import org.xtreemfs.osd.stages.PreprocStage.InvalidateXLocSetCallback;
import org.xtreemfs.osd.stages.StorageStage.InternalGetReplicaStateCallback;
import org.xtreemfs.pbrpc.generatedinterfaces.OSD.ReplicaStatus;
import org.xtreemfs.pbrpc.generatedinterfaces.OSD.xtreemfs_xloc_set_invalidateRequest;
import org.xtreemfs.pbrpc.generatedinterfaces.OSD.xtreemfs_xloc_set_invalidateResponse;
import org.xtreemfs.pbrpc.generatedinterfaces.OSDServiceConstants;

public class InvalidateXLocSetOperation extends OSDOperation {
    final String      sharedSecret;
    final ServiceUUID localUUID;

    public InvalidateXLocSetOperation(OSDRequestDispatcher master) {
        super(master);
        sharedSecret = master.getConfig().getCapabilitySecret();
        localUUID = master.getConfig().getUUID();

    }

    @Override
    public int getProcedureId() {
        return OSDServiceConstants.PROC_ID_XTREEMFS_XLOC_SET_INVALIDATE;
    }

    @Override
    public void startRequest(final OSDRequest rq) {
        master.getPreprocStage().invalidateXLocSet(rq, new InvalidateXLocSetCallback() {
            
            @Override
            public void invalidateComplete(boolean isPrimary, ErrorResponse error) {
                if (error != null) {
                    rq.sendError(error);
                } else {
                    postInvalidation(rq, isPrimary);
                }
            }
        });
    }

    private void postInvalidation(final OSDRequest rq, final boolean isPrimary) {
        // TODO (jdillmann): RO replication
        if (rq.getLocationList().getReplicaUpdatePolicy().equals(ReplicaUpdatePolicies.REPL_UPDATE_PC_RONLY)) {
            invalidationFinished(rq, isPrimary, null);
        } else {
            // TODO (jdillmann): check maxLocalObjVersion parameter @see CoordinatedReplicaUpdatePolicy.executeReset
            master.getStorageStage().internalGetReplicaState(rq.getFileId(),
                    rq.getLocationList().getLocalReplica().getStripingPolicy(), 0,
                    new InternalGetReplicaStateCallback() {

                        @Override
                        public void getReplicaStateComplete(ReplicaStatus localState, ErrorResponse error) {
                            if (error != null) {
                                rq.sendError(error);
                            } else {
                                invalidationFinished(rq, isPrimary, localState);
                            }
                        }
                    });
        }
    }

    private void invalidationFinished(OSDRequest rq, boolean isPrimary, ReplicaStatus localState) {
        xtreemfs_xloc_set_invalidateResponse.Builder response = xtreemfs_xloc_set_invalidateResponse.newBuilder();
        response.setIsPrimary(isPrimary);

        if (localState != null) {
            response.setStatus(localState);
        }
        
        rq.sendSuccess(response.build(), null);
    }

    @Override
    public void startInternalEvent(Object[] args) {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    @Override
    public ErrorResponse parseRPCMessage(OSDRequest rq) {
        try {
            xtreemfs_xloc_set_invalidateRequest rpcrq = (xtreemfs_xloc_set_invalidateRequest) rq.getRequestArgs();
            rq.setFileId(rpcrq.getFileId());
            rq.setCapability(new Capability(rpcrq.getFileCredentials().getXcap(), sharedSecret));
            rq.setLocationList(new XLocations(rpcrq.getFileCredentials().getXlocs(), localUUID));

            return null;
        } catch (InvalidXLocationsException ex) {
            return ErrorUtils.getErrorResponse(ErrorType.ERRNO, POSIXErrno.POSIX_ERROR_EINVAL, ex.toString());
        } catch (Throwable ex) {
            return ErrorUtils.getInternalServerError(ex);
        }
    }

    @Override
    public boolean requiresCapability() {
        return true;
    }

    @Override
    public boolean requiresValidView() {
        // Although the local view will invalidated it is required to check the request. This will ensure, that the
        // local version equals the latest one.
        // If this is not guaranteed, it could be that 'local version' < 'current version' < 'next version' and a
        // request from an outdated client with 'current version' would reset the INVALIDATED flag and be successful.
        return true;
    }
}
