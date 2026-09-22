# CoDel_Simulation_NS3
Code Mạng máy tính 2
# ns-3 Queue Performance: FIFO vs CoDel in Dumbbell Topology

## 📌 Tổng quan
Dự án này là một kịch bản mô phỏng mạng sử dụng **ns-3**, được thiết kế để đánh giá và so sánh hiệu năng giữa hai thuật toán quản lý hàng đợi: **FIFO (First-In-First-Out)** và **CoDel (Controlled Delay)**. Hệ thống đo đạc ảnh hưởng của các kích thước bộ đệm (queue size) khác nhau lên thông lượng (throughput), độ trễ (delay) và tỷ lệ rớt gói (packet loss), qua đó minh họa rõ nét hiện tượng **Bufferbloat**.

## 🏗️ Cấu hình mạng (Topology)
Mô phỏng sử dụng mô hình mạng **Dumbbell** cơ bản:
* **Bottleneck Link (Nút thắt cổ chai):** Nằm giữa 2 Router với cấu hình băng thông `10Mbps` và độ trễ `20ms`.
* **Access Links:** Các nút nguồn (`nLeft`) và nút đích (`nRight`) kết nối vào router tương ứng với băng thông `10Mbps` và độ trễ `0.1ms`.
* **Lưu lượng (Traffic):** Sử dụng các ứng dụng TCP On/Off để mô phỏng truyền tải dữ liệu liên tục (thông số Packet Size = 1472, Segment Size = 1000).

## ⚙️ Tính năng chính
* **Kiểm thử đa biến:** Mã nguồn tự động chạy vòng lặp đánh giá cả hai thuật toán (`Fifo`, `CoDel`) qua mảng các kích thước bộ đệm cấu hình sẵn: `5, 10, 50, 100, 200, 350, 500, 1000` packets.
* **Ghi log cấp độ gói tin (Tracing):** Theo dõi trực tiếp các sự kiện `Enqueue` và `Dequeue` tại nút thắt cổ chai. Dữ liệu được xuất tự động ra các file `.csv` riêng biệt, cung cấp tập dữ liệu thô lý tưởng cho việc vẽ biểu đồ và đưa vào các báo cáo kỹ thuật.
* **Thống kê tổng quan (FlowMonitor):** Tự động tính toán và in ra terminal các số liệu phân tích mạng cốt lõi sau mỗi chu kỳ chạy (Tx/Rx Packets, Lost Packets, Throughput, Mean Delay).

## 🚀 Hướng dẫn chạy
**Yêu cầu:** Đã cài đặt [ns-3](https://www.nsnam.org/) (khuyến nghị phiên bản hỗ trợ `ns3::CoDelQueueDisc`).

1. Copy file mã nguồn (ví dụ: `dumbbell-queue.cc`) vào thư mục `scratch/` của bộ mã nguồn ns-3.
2. Mở terminal tại thư mục gốc của ns-3 và chạy lệnh sau (có thể tùy chỉnh số lượng nút trái/phải):

```bash
./ns3 run "scratch/dumbbell-queue --nLeft=3 --nRight=3"
