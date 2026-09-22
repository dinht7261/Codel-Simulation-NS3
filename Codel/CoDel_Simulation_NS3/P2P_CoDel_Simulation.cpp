#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include <vector>
#include <string>
#include <iomanip>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellQueueTerminal");

// --- BIẾN TOÀN CỤC PHỤC VỤ LOGGING ---
std::ofstream g_csvFile;
uint32_t g_totalEnqueues = 0;
std::string g_currentQueueType = "";

// --- TRACE CALLBACKS CHO HÀNG ĐỢI ---
void EnqueueCallback(Ptr<const QueueDiscItem> item)
{
    Time now = Simulator::Now();
    uint64_t uid = item->GetPacket()->GetUid();
    g_totalEnqueues++;

    if (g_csvFile.is_open()) {
        g_csvFile << "ENQUEUE," << now.GetSeconds() << "," << uid << "," << g_totalEnqueues << "\n";
    }
}

void DequeueCallback(Ptr<const QueueDiscItem> item)
{
    Time now = Simulator::Now();
    uint64_t uid = item->GetPacket()->GetUid();

    if (g_csvFile.is_open()) {
        g_csvFile << "DEQUEUE," << now.GetSeconds() << "," << uid << ",\n";
        g_csvFile << g_currentQueueType << " OK," << now.GetSeconds() << "," << uid << ",\n";
    }
}

void RunScenario(std::string queueDiscType, 
                 uint32_t nLeft, uint32_t nRight,
                 std::vector<std::string> leftBw, std::vector<std::string> leftDelay,
                 std::vector<std::string> rightBw, std::vector<std::string> rightDelay,
                 std::string botBw, std::string botDelay, 
                 float simDuration, uint32_t queueSize)
{
    g_currentQueueType = queueDiscType;

    std::cout << "\n====================================================================\n";
    std::cout << " BẮT ĐẦU CHẠY: Thuật toán " << queueDiscType << " | QueueSize: " << queueSize << "p\n";
    std::cout << "====================================================================\n";

    g_totalEnqueues = 0;
    std::ostringstream csvName;
    csvName << "log_" << queueDiscType << "_" << queueSize << ".csv";
    g_csvFile.open(csvName.str().c_str());
    if (g_csvFile.is_open()) {
        g_csvFile << "Event,Time(s),UID,TotalEnqueues\n";
    }

    NodeContainer routers;
    routers.Create(2);

    NodeContainer leftLeaves;
    leftLeaves.Create(nLeft);

    NodeContainer rightLeaves;
    rightLeaves.Create(nRight);

    InternetStackHelper stack;
    stack.Install(routers);
    stack.Install(leftLeaves);
    stack.Install(rightLeaves);

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(botBw));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(botDelay));
    bottleneckLink.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1p")); 

    std::ostringstream qSizeStr;
    qSizeStr << queueSize << "p";

    TrafficControlHelper tchBottleneck;
    if (queueDiscType == "Fifo") {
        tchBottleneck.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue(qSizeStr.str()));
    } else if (queueDiscType == "CoDel") {
        // Đã sửa đổi: Gán MaxSize động cho CoDel theo từng vòng lặp queueSize
        tchBottleneck.SetRootQueueDisc("ns3::CoDelQueueDisc", "MaxSize", StringValue(qSizeStr.str()));
    }

    NetDeviceContainer botDevs = bottleneckLink.Install(routers.Get(0), routers.Get(1));
    QueueDiscContainer botQueueDiscs = tchBottleneck.Install(botDevs);

    if (botQueueDiscs.GetN() > 0) {
        Ptr<QueueDisc> qd = botQueueDiscs.Get(0);
        qd->TraceConnectWithoutContext("Enqueue", MakeCallback(&EnqueueCallback));
        qd->TraceConnectWithoutContext("Dequeue", MakeCallback(&DequeueCallback));
    }

    Ipv4AddressHelper address;
    uint32_t subnet = 1;
    
    std::ostringstream botSubnetStr;
    botSubnetStr << "10.1." << subnet++ << ".0";
    address.SetBase(botSubnetStr.str().c_str(), "255.255.255.0");
    Ipv4InterfaceContainer botInterfaces = address.Assign(botDevs);

    TrafficControlHelper tchAccess;
    tchAccess.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue("1000p"));

    std::vector<Ipv4InterfaceContainer> leftInterfaces;
    std::vector<Ipv4InterfaceContainer> rightInterfaces;

    for (uint32_t i = 0; i < nLeft; ++i) {
        PointToPointHelper accessLink;
        accessLink.SetDeviceAttribute("DataRate", StringValue(leftBw[i]));
        accessLink.SetChannelAttribute("Delay", StringValue(leftDelay[i]));
        accessLink.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("50p"));

        NetDeviceContainer devs = accessLink.Install(leftLeaves.Get(i), routers.Get(0));
        tchAccess.Install(devs);

        std::ostringstream subnetStr;
        subnetStr << "10.1." << subnet++ << ".0";
        address.SetBase(subnetStr.str().c_str(), "255.255.255.0");
        leftInterfaces.push_back(address.Assign(devs));
    }

    for (uint32_t i = 0; i < nRight; ++i) {
        PointToPointHelper accessLink;
        accessLink.SetDeviceAttribute("DataRate", StringValue(rightBw[i]));
        accessLink.SetChannelAttribute("Delay", StringValue(rightDelay[i]));
        accessLink.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("50p"));

        NetDeviceContainer devs = accessLink.Install(routers.Get(1), rightLeaves.Get(i));
        tchAccess.Install(devs);

        std::ostringstream subnetStr;
        subnetStr << "10.1." << subnet++ << ".0";
        address.SetBase(subnetStr.str().c_str(), "255.255.255.0");
        rightInterfaces.push_back(address.Assign(devs));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 5000;
    for (uint32_t i = 0; i < nLeft; ++i) {
        uint32_t destIndex = i % nRight; 
        
        Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sinkApps = packetSinkHelper.Install(rightLeaves.Get(destIndex));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(simDuration));

        InetSocketAddress remoteAddress = InetSocketAddress(rightInterfaces[destIndex].GetAddress(1), port);
        OnOffHelper onoff("ns3::TcpSocketFactory", Address(remoteAddress));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", StringValue(leftBw[i])); 
        
        uint32_t packetSize = 1472;
        uint32_t targetPackets = 500; // Maxpackets
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onoff.SetAttribute("MaxBytes", UintegerValue(packetSize * targetPackets)); 
        
        ApplicationContainer clientApps = onoff.Install(leftLeaves.Get(i));
        clientApps.Start(Seconds(0.1 + (i * 0.02))); 
        clientApps.Stop(Seconds(simDuration - 0.1));
        
        port++;
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    if (g_csvFile.is_open()) {
        g_csvFile.close();
    }

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n--- THỐNG KÊ KẾT QUẢ CHO: " << queueDiscType << " (QueueSize: " << queueSize << ") ---\n";
    std::cout << std::left << std::setw(8) << "Flow ID" 
              << std::setw(25) << "Src -> Dst" 
              << std::setw(12) << "Tx Pkts" 
              << std::setw(12) << "Rx Pkts" 
              << std::setw(12) << "Lost Pkts" 
              << std::setw(15) << "Throughput" 
              << std::setw(15) << "Mean Delay" << "\n";
    std::cout << "-------------------------------------------------------------------------------------------------\n";

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        
        if (t.destinationPort >= 5000 || t.sourcePort >= 5000) {
            double timeSpan = i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds();
            double throughput = 0;
            if (timeSpan > 0 && i->second.rxPackets > 0) {
                throughput = (i->second.rxBytes * 8.0) / (timeSpan * 1000 * 1000); 
            }
            double meanDelay = 0;
            if (i->second.rxPackets > 0) {
                meanDelay = i->second.delaySum.GetSeconds() / i->second.rxPackets * 1000; 
            }

            std::ostringstream srcDst;
            srcDst << t.sourceAddress << " -> " << t.destinationAddress;

            std::cout << std::left << std::setw(8) << i->first 
                      << std::setw(25) << srcDst.str()
                      << std::setw(12) << i->second.txPackets 
                      << std::setw(12) << i->second.rxPackets 
                      << std::setw(12) << i->second.lostPackets 
                      << std::fixed << std::setprecision(6) << throughput << " Mbps     " 
                      << meanDelay << " ms\n";
        }
    }
    std::cout << "-------------------------------------------------------------------------------------------------\n\n";

    Simulator::Destroy();
}

int main(int argc, char* argv[])
{
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1000));
    
    uint32_t nLeft = 3;  
    uint32_t nRight = 3; 
    
    std::vector<std::string> leftBw    = {"10Mbps", "10Mbps", "10Mbps"};
    std::vector<std::string> leftDelay = {"0.1ms", "0.1ms", "0.1ms"};

    std::vector<std::string> rightBw    = {"10Mbps", "10Mbps", "10Mbps"};
    std::vector<std::string> rightDelay = {"0.1ms", "0.1ms", "0.1ms"};

    std::string botBw = "10Mbps"; 
    std::string botDelay = "20ms";
    float simDuration = 60.0; 

    CommandLine cmd(__FILE__);
    cmd.AddValue("nLeft", "Số lượng node trái", nLeft);
    cmd.AddValue("nRight", "Số lượng node phải", nRight);
    cmd.Parse(argc, argv);

    std::vector<uint32_t> queueSizes = {5, 10, 50, 100, 200, 350, 500, 1000};
    
    // THAY ĐỔI: Thêm "CoDel" vào danh sách hàng đợi cần chạy để xử lý cả 2 thuật toán
    std::vector<std::string> targetQueues = {"Fifo", "CoDel"};
    
    for (const auto& qDisc : targetQueues) {
        for (uint32_t qs : queueSizes) {
            RunScenario(qDisc, nLeft, nRight, leftBw, leftDelay, rightBw, rightDelay, botBw, botDelay, simDuration, qs);
        }
    }

    std::cout << "HOÀN TẤT TOÀN BỘ TIẾN TRÌNH!\n";
    return 0;
}