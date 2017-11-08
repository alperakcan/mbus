
import java.io.*;
import java.net.*;
import java.util.Arrays;
 
public class MBusClient {
    	public static void main (String[] args) throws IOException {
		if (args.length != 2) {
            		System.err.println("Usage: java MBusClient <host name> <port number>");
            		System.exit(1);
        	}
		String hostName = args[0];
        	int portNumber = Integer.parseInt(args[1]);
        	try (
            		Socket socket = new Socket(hostName, portNumber);
            		DataOutputStream out = new DataOutputStream(socket.getOutputStream());
            		DataInputStream in = new DataInputStream(socket.getInputStream());
        	) {
            		String requestString;
            		byte[] requestStringBytes;
            		byte[] requestMessage;

            		requestString = String.format("{\"type\":\"org.mbus.method.type.command\",\"source\":\"\",\"destination\":\"org.mbus.server\",\"identifier\":\"command.call\",\"sequence\":1,\"payload\":{\"destination\":\"org.mbus.server\",\"identifier\":\"command.create\",\"call\":{}}}");
        		
        		requestStringBytes = requestString.getBytes();
        		requestMessage = new byte[requestStringBytes.length + 4];
        		
        		requestMessage[0] = (byte) ((requestStringBytes.length >> 0x18) & 0xFF);
        		requestMessage[1] = (byte) ((requestStringBytes.length >> 0x10) & 0xFF);
        		requestMessage[2] = (byte) ((requestStringBytes.length >> 0x08) & 0xFF);
	        	requestMessage[3] = (byte) ((requestStringBytes.length >> 0x00) & 0xFF);
 			for (int i = 0, j = 4; i < requestStringBytes.length; i++, j++) {
	            		requestMessage[j] = requestStringBytes[i];
        		}
            		out.write(requestMessage);

            		requestString = String.format("{\"type\":\"org.mbus.method.type.command\",\"source\":\"\",\"destination\":\"org.mbus.server\",\"identifier\":\"command.subscribe\",\"sequence\":2,\"payload\":{\"source\":\"org.mbus.method.event.source.all\",\"event\":\"event.deneme\"}}");
        		
        		requestStringBytes = requestString.getBytes();
        		requestMessage = new byte[requestStringBytes.length + 4];
        		
        		requestMessage[0] = (byte) ((requestStringBytes.length >> 0x18) & 0xFF);
        		requestMessage[1] = (byte) ((requestStringBytes.length >> 0x10) & 0xFF);
        		requestMessage[2] = (byte) ((requestStringBytes.length >> 0x08) & 0xFF);
	        	requestMessage[3] = (byte) ((requestStringBytes.length >> 0x00) & 0xFF);
 			for (int i = 0, j = 4; i < requestStringBytes.length; i++, j++) {
	            		requestMessage[j] = requestStringBytes[i];
        		}
            		out.write(requestMessage);

			int responseBufferLength;
			int responseBufferSize;
			byte[] responseBuffer;
			
            		responseBufferLength = 0;
			responseBufferSize = 1;
			responseBuffer = new byte[responseBufferSize];
			
            		while (true) {
            			int rc;
	            		
	            		rc = in.read(responseBuffer, responseBufferLength, responseBufferSize - responseBufferLength);
	            		if (rc <= 0) {
	            			break;
	            		}
	            		responseBufferLength += rc;
	            		while (responseBufferLength >= responseBufferSize / 2) {
					byte[] responseBufferGrow;
					responseBufferGrow = new byte[responseBufferSize * 2];
					System.arraycopy(responseBuffer, 0, responseBufferGrow, 0, responseBufferLength);
					responseBuffer = responseBufferGrow;
					responseBufferSize *= 2;
	            		}
	            		if (responseBufferLength < 4) {
	            			continue;
	            		}
	            		
	            		int expected;
	            		expected  = (responseBuffer[0] & 0xFF) << 0x18;
                		expected |= (responseBuffer[1] & 0xFF) << 0x10;
                		expected |= (responseBuffer[2] & 0xFF) << 0x08;
                		expected |= (responseBuffer[3] & 0xFF) << 0x00;

	            		if (responseBufferLength < 4 + expected) {
	            			continue;
	            		}
                		
                		byte[] responseMessage;
                		String responseString;
                		responseMessage = Arrays.copyOfRange(responseBuffer, 4, 4 + expected);
                		responseString = new String(responseMessage);
                		
                		System.out.println("expected: " + expected);
                		System.out.println("responseString: " + responseString);
                		
                		System.arraycopy(responseBuffer, 4 + expected, responseBuffer, 0, responseBufferLength - 4 - expected);
                		responseBufferLength -= expected;
                		responseBufferLength -= 4;
            		}
            		
		} catch (UnknownHostException e) {
            		System.err.println("Don't know about host " + hostName);
            		System.exit(1);
        	} catch (IOException e) {
            		System.err.println("Couldn't get I/O for the connection to " + hostName);
            		System.exit(1);
        	} 
    	}
}