package org.xtreemfs.interfaces.MRCInterface;

import org.xtreemfs.*;
import org.xtreemfs.common.buffer.ReusableBuffer;
import org.xtreemfs.interfaces.*;
import org.xtreemfs.interfaces.utils.*;
import yidl.Marshaller;
import yidl.Struct;
import yidl.Unmarshaller;




public class openResponse extends org.xtreemfs.interfaces.utils.Response
{
    public static final int TAG = 2009082829;
    
    public openResponse() { file_credentials = new FileCredentials();  }
    public openResponse( FileCredentials file_credentials ) { this.file_credentials = file_credentials; }

    public FileCredentials getFile_credentials() { return file_credentials; }
    public void setFile_credentials( FileCredentials file_credentials ) { this.file_credentials = file_credentials; }

    // java.io.Serializable
    public static final long serialVersionUID = 2009082829;    

    // yidl.Object
    public int getTag() { return 2009082829; }
    public String getTypeName() { return "org::xtreemfs::interfaces::MRCInterface::openResponse"; }
    
    public int getXDRSize()
    {
        int my_size = 0;
        my_size += file_credentials.getXDRSize();
        return my_size;
    }    
    
    public void marshal( Marshaller marshaller )
    {
        marshaller.writeStruct( "file_credentials", file_credentials );
    }
    
    public void unmarshal( Unmarshaller unmarshaller ) 
    {
        file_credentials = new FileCredentials(); unmarshaller.readStruct( "file_credentials", file_credentials );    
    }
        
    

    private FileCredentials file_credentials;    

}

