/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package org.xtreemfs.dir.data;

import java.io.IOException;
import java.nio.BufferOverflowException;
import java.nio.BufferUnderflowException;
import java.util.ArrayList;
import java.util.List;
import org.xtreemfs.common.buffer.ReusableBuffer;
import org.xtreemfs.interfaces.AddressMapping;
import org.xtreemfs.interfaces.AddressMappingSet;

/**
 *
 * @author bjko
 */
public class AddressMappingRecords {

    ArrayList<AddressMappingRecord> records;

    public AddressMappingRecords() {
        records = new ArrayList<AddressMappingRecord>(2);
    }

    public AddressMappingRecords(ReusableBuffer rb) throws IOException {
        try {
            int numEntries = rb.getInt();
            records = new ArrayList<AddressMappingRecord>(numEntries);
            for (int i = 0; i < numEntries; i++) {
                AddressMappingRecord m = new AddressMappingRecord(rb);
                records.add(m);
            }
        } catch (BufferUnderflowException ex) {
            throw new IOException("corrupted AddressMappingRecords entry: "+ex,ex);
        }
    }

    public AddressMappingRecords(AddressMappingSet set) {
        records = new ArrayList<AddressMappingRecord>(set.size());
        for (AddressMapping am : set) {
            records.add(new AddressMappingRecord(am));
        }
    }

    public AddressMappingSet getAddressMappingSet() {
        AddressMappingSet set = new AddressMappingSet();
        for (AddressMappingRecord rec : records) {
            set.add(rec.getAddressMapping());
        }
        return set;
    }

    public void add(AddressMappingRecords otherList) {
        records.addAll(otherList.records);
    }

    public int size() {
        return records.size();
    }

    public AddressMappingRecord getRecord(int index) {
        return records.get(index);
    }

    public List<AddressMappingRecord> getRecords() {
        return records;
    }

    public int getSize() {
        final int INT_SIZE = Integer.SIZE/8;
        int size = INT_SIZE;
        for (AddressMappingRecord rec: records) {
            size += rec.getSize();
        }
        return size;
    }

    public void serialize(ReusableBuffer rb) throws IOException {
        try {
            rb.putInt(records.size());
            for (AddressMappingRecord rec: records) {
                rec.serialize(rb);
            }
        } catch (BufferOverflowException ex) {
            throw new IOException("buffer too small",ex);
        }
    }

}
