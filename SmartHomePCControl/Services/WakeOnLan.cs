using System.Net;
using System.Net.Sockets;
using System.Text;

namespace SmartHomePCControl.Services;

public static class WakeOnLan
{
    public static void SendMagicPacket(string macAddress)
    {
        // Validate MAC address and remove any non-hexadecimal characters.
        macAddress = macAddress.Replace(":", string.Empty).Replace("-", string.Empty);
        if (macAddress.Length != 12)
            throw new ArgumentException("Invalid MAC address format");

        // Convert MAC address string to byte array
        var macBytes = Enumerable.Range(0, macAddress.Length / 2)
            .Select(x => Convert.ToByte(macAddress.Substring(x * 2, 2), 16))
            .ToArray();

        // Construct magic packet
        var magicPacket = new byte[6 + 16 * macBytes.Length];
        for (var i = 0; i < 6; i++) magicPacket[i] = 0xFF;

        for (var i = 1; i <= 16; i++) Array.Copy(macBytes, 0, magicPacket, i * macBytes.Length, macBytes.Length);

        // Send magic packet
        using var client = new UdpClient();
        client.Connect(IPAddress.Broadcast, 9);
        client.Send(magicPacket, magicPacket.Length);
    }

    public static void SendUdpShutdownCommand(string ipAddress, int port)
    {
        try
        {
            // Create a UdpClient instance
            using var udpClient = new UdpClient();
            // Convert the shutdown command to bytes
            byte[] commandBytes = "shutdown-my-pc"u8.ToArray();
            // Send the command to the specified IP address and port
            udpClient.Send(commandBytes, commandBytes.Length, ipAddress, port);
        }
        catch (Exception ex)
        {
            // Handle any exceptions, such as network errors
            Console.WriteLine($"Error occurred while sending shutdown command: {ex.Message}");
        }
    }

    public static void SendTcpShutdownCommand(string ipAddress, int port)
    {
        try
        {
            // Create a TCP client
            using TcpClient client = new TcpClient(ipAddress, port);
            // Convert the message to bytes
            string message = "shutdown-my-pc";
            byte[] data = Encoding.ASCII.GetBytes(message);

            // Get the network stream
            NetworkStream stream = client.GetStream();

            // Send the message
            stream.Write(data, 0, data.Length);

            Console.WriteLine("Message sent: " + message);
        }
        catch (Exception ex)
        {
            Console.WriteLine("Error: " + ex.Message);
        }
    }

    // Define a method to check if the PC is on/off based on socket port
    public static bool IsPCOn(string ipAddress, int port)
    {
        using var tcpClient = new TcpClient();
        try
        {
            // Attempt to connect to the PC's IP address and socket port
            tcpClient.SendTimeout = 500;
            tcpClient.ReceiveTimeout = 500;
            tcpClient.Connect(ipAddress, port);
            // If connection succeeds, port 3389 is open (PC is on)
            return true;
        }
        catch (SocketException)
        {
            // If connection fails, port 3389 is closed (PC is off or unreachable)
            return false;
        }
    }

    public static void SendWakeUpSignal(string ipAddress, int port)
    {
        try
        {
            // Create a TCP client
            using TcpClient client = new TcpClient();
            // Connect to the server
            client.Connect(IPAddress.Parse(ipAddress), port);

            // Send a wake-up message
            string message = "wake-my-pc-up";
            byte[] data = Encoding.ASCII.GetBytes(message);
            NetworkStream stream = client.GetStream();
            stream.Write(data, 0, data.Length);
            Console.WriteLine("Wake-up signal sent successfully.");
        }
        catch (Exception ex)
        {
            Console.WriteLine("Error sending wake-up signal: " + ex.Message);
        }
    }
}