using JetBrains.Annotations;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using SmartHomePCControl.Services;

namespace SmartHomePCControl.Tests.Services;

[TestClass]
[TestSubject(typeof(WakeOnLan))]
public class WakeOnLanTest
{
    [TestMethod]
    public void TestWakeUpCommand()
    {
        WakeOnLan.SendMagicPacket("58:11:22:c8:57:67");
    }

    [TestMethod]
    public void TestShutDownCommand()
    {
        WakeOnLan.SendShutdownCommand("192.168.97.3", 10675);
    }
}