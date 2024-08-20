using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;

namespace SmartHomePCControl.Services
{
    public class WakeOnLanService
    {
        private static readonly ILogger<WakeOnLanService> _logger =
            LoggerFactory.Create(builder => builder.AddConsole()).CreateLogger<WakeOnLanService>();

        public enum TcpCommand
        {
            Shutdown,
            WakeUp,
            Reboot
        }

        private const string ShutdownCommand = "shutdown-my-pc";
        private const string WakeUpCommand = "wake-up-my-pc";
        private const string RebootCommand = "reboot-my-pc";

        // Define a method to check if the PC is on/off based on socket port
        public async Task<bool> IsPCOnAsync(string ipAddress, int port)
        {
            using var tcpClient = new TcpClient();
            try
            {
                tcpClient.SendTimeout = 500;
                tcpClient.ReceiveTimeout = 500;
                await tcpClient.ConnectAsync(ipAddress, port);
                // If connection succeeds, port is open (PC is on)
                return true;
            }
            catch (SocketException)
            {
                // If connection fails, port is closed (PC is off or unreachable)
                return false;
            }
        }

        public void SendMagicPacket(string macAddress)
        {
            // Validate MAC address and remove any non-hexadecimal characters.
            macAddress = macAddress.Replace(":", string.Empty).Replace("-", string.Empty);
            if (macAddress.Length != 12)
                throw new ArgumentException("Invalid MAC address format");

            Span<byte> macBytes = stackalloc byte[6];
            for (int i = 0; i < 6; i++)
            {
                macBytes[i] = Convert.ToByte(macAddress.Substring(i * 2, 2), 16);
            }

            Span<byte> magicPacket = stackalloc byte[102];
            magicPacket.Slice(0, 6).Fill(0xFF);

            for (int i = 1; i <= 16; i++)
            {
                macBytes.CopyTo(magicPacket.Slice(i * 6, 6));
            }

            using var client = new UdpClient();
            client.Connect(IPAddress.Broadcast, 9);
            client.Send(magicPacket.ToArray(), magicPacket.Length);
        }

        public void SendUdpShutdownCommand(string ipAddress, int port)
        {
            try
            {
                // Create a UdpClient instance
                using var udpClient = new UdpClient();
                // Convert the shutdown command to bytes
                byte[] commandBytes = Encoding.UTF8.GetBytes(ShutdownCommand);
                // Send the command to the specified IP address and port
                udpClient.Send(commandBytes, commandBytes.Length, ipAddress, port);
            }
            catch (SocketException ex)
            {
                // Handle network errors
                _logger.LogError("Network error while sending shutdown command: {Message}", ex.Message);
            }
            catch (Exception ex)
            {
                // Handle other errors
                _logger.LogError("Error occurred while sending shutdown command: {Message}", ex.Message);
            }
        }

        public void SendTcpCommand(string ipAddress, int port, TcpCommand command)
        {
            string message = command switch
            {
                TcpCommand.Shutdown => ShutdownCommand,
                TcpCommand.WakeUp => WakeUpCommand,
                TcpCommand.Reboot => RebootCommand,
                _ => throw new ArgumentException("Invalid command"),
            };

            SendTcpMessageAsync(ipAddress, port, message).Wait();
        }

        private async Task SendTcpMessageAsync(string ipAddress, int port, string message)
        {
            try
            {
                // Create a TCP client
                using TcpClient client = new TcpClient();
                // Connect to the server
                await client.ConnectAsync(IPAddress.Parse(ipAddress), port);
                byte[] data = Encoding.ASCII.GetBytes(message);
                NetworkStream stream = client.GetStream();
                await stream.WriteAsync(data, 0, data.Length);
                _logger.LogInformation("TCP message sent successfully");
            }
            catch (SocketException ex)
            {
                _logger.LogError("Network error: {Message}", ex.Message);
            }
            catch (IOException ex)
            {
                _logger.LogError("I/O error: {Message}", ex.Message);
            }
            catch (Exception ex)
            {
                _logger.LogError("Error sending TCP message: {Message}", ex.Message);
            }
        }
    }
}