package org.xtreemfs.interfaces.MRCInterface;

import org.xtreemfs.*;
import org.xtreemfs.common.buffer.ReusableBuffer;
import org.xtreemfs.interfaces.*;
import org.xtreemfs.interfaces.utils.*;
import yidl.Marshaller;
import yidl.Struct;
import yidl.Unmarshaller;




public class setattrRequest extends org.xtreemfs.interfaces.utils.Request
{
    public static final int TAG = 2009082834;
    
    public setattrRequest() { stbuf = new Stat();  }
    public setattrRequest( String path, Stat stbuf ) { this.path = path; this.stbuf = stbuf; }

    public String getPath() { return path; }
    public void setPath( String path ) { this.path = path; }
    public Stat getStbuf() { return stbuf; }
    public void setStbuf( Stat stbuf ) { this.stbuf = stbuf; }

    // Request
    public Response createDefaultResponse() { return new setattrResponse(); }


    // java.io.Serializable
    public static final long serialVersionUID = 2009082834;    

    // yidl.Object
    public int getTag() { return 2009082834; }
    public String getTypeName() { return "org::xtreemfs::interfaces::MRCInterface::setattrRequest"; }
    
    public int getXDRSize()
    {
        int my_size = 0;
        my_size += ( ( path.getBytes().length + Integer.SIZE/8 ) % 4 == 0 ) ? ( path.getBytes().length + Integer.SIZE/8 ) : ( path.getBytes().length + Integer.SIZE/8 + 4 - ( path.getBytes().length + Integer.SIZE/8 ) % 4 );
        my_size += stbuf.getXDRSize();
        return my_size;
    }    
    
    public void marshal( Marshaller marshaller )
    {
        marshaller.writeString( "path", path );
        marshaller.writeStruct( "stbuf", stbuf );
    }
    
    public void unmarshal( Unmarshaller unmarshaller ) 
    {
        path = unmarshaller.readString( "path" );
        stbuf = new Stat(); unmarshaller.readStruct( "stbuf", stbuf );    
    }
        
    

    private String path;
    private Stat stbuf;    

}

