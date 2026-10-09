# XXCL - Proje Mimarisi ve Teknik Dokümantasyon (Architecture Sync)

Bu doküman, Google Antigravity ve XXCL uyumluluk katmanı projeleri arasındaki kurallar çerçevesinde otomatik güncellenen yapı ve mimari özetidir. (Rule 6 gereği senkronize edilmiştir).

## 1. Dizin ve Dosya Yapısı

```
/home/hakan/Desktop/xxcl-bin/
├── xbe-inspect/                # Ana emülatör / uyumluluk katmanı kaynak dizini
│   ├── src/
│   │   ├── loader.cpp          # XBE dosyasını ayrıştıran ve belleğe yükleyen motor
│   │   ├── kernel/             # Xbox Kernel (__stdcall) fonksiyonları (HLE)
│   │   │   ├── ex.cpp          # Executive (Heap, Pool, EEPROM vb.)
│   │   │   ├── mm.cpp          # Memory Manager (mmap, Physical Address mask vb.)
│   │   │   ├── hal.cpp         # Hardware Abstraction Layer & SIGSEGV Handler
│   │   │   ├── nv2a.cpp        # NV2A GPU & PushBuffer asenkron worker
│   │   │   ├── io.cpp          # I/O System (VFS taklitleri)
│   │   │   ├── ps.cpp          # Process/Thread yönetimi
│   │   │   ├── ke.cpp          # Kernel Exec (Interrupts, DPC, vb.)
│   │   │   └── rtl.cpp         # Runtime Library (Kritik Bölümler vb.)
│   │   ├── video/              # Vulkan / OpenGL stubları (gelecek için)
│   │   └── main.cpp            # Uygulama giriş noktası ve CLI
│   └── CMakeLists.txt          # Yapılandırma dosyası
├── build/                      # Derlenmiş binary (xbe-inspect)
└── README.md                   # Kullanıcıya yönelik genel proje ve ilerleme durumu
```

## 2. Bellek Haritası ve Yaklaşımlar (Memory Map Strategy)

Orijinal Xbox 64MB RAM'e ve spesifik donanım MMIO aralıklarına sahiptir. XXCL bu bellek haritasını Linux üzerinde `mmap(MAP_FIXED)` kullanarak birebir yansıtmaya çalışır.

- **0x00010000 - 0x0xxxxxxx**: XBE Native Code (.text, .rdata, vb.) alanı.
- **0xFD000000 - 0xFDFFFFFF**: NV2A (GPU) Register ve Framebuffer erişim alanları (Worker thread veya Handler ile yönetilir).
- **0xFE800000**: APU (Audio Processing Unit) DSP bellek alanı. (Sega GT'nin çökmesini engellemek için allocate edilip 0'lanır).
- **0xFEC00000**: APIC (Gelişmiş Kesme Kontrolcüsü). (Belleğe map edilmiştir).
- **Physical Address Mask**: Oyun kodu sanal bir bellek adresi olan örneğin `0xB02A0000`'ı fiziksel adrese (DMA için vs.) dönüştürürken `MmGetPhysicalAddress` çağrısı `addr & 0x03FFFFFF` maskesini uygular (Linux MMU ve Xbox MMU farkını gizlemek için).

## 3. Asenkron Donanım Simülasyonu (Puller/Worker)
Oyunlar donanıma statik bayrak (flag) yazıp donanımın bu bayrağı (örneğin FIFO boş, Frame hazır) sıfırlamasını beklerler (Spin-Loop).
- **Çözüm (nv2a.cpp)**: XXCL, `pthread` tabanlı arka plan iş parçacıkları (worker thread) çalıştırır. `0xFD100410` (NV2A FIFO) ve benzeri registerları periyodik olarak süpürüp "meşgul" bayrağını kaldırarak oyun motorunun kilitlenmesini çözer.

## 4. İstisnai Durum Yakalama (Exception Handling)
İşletim sistemi seviyesindeki donanımsal okumalar (`in/out` portları) ve çekirdek seviyesi ayrıcalıklı komutlar (örn. `WBINVD` `0F 09`), Linux kullanıcı uzayında anında `SIGSEGV` üretir.
- **Çözüm (loader.cpp / hal.cpp)**: Özel bir `sigaction` kurulmuştur. Instruction Pointer (EIP) incelenerek eğer `0xEE` (`out dx, al`) gibi bilinen zararsız (veya uyumluluk katmanı için mocklanabilir) bir komutsa, Instruction Pointer bayt boyutu kadar atlanarak (`EIP += 1/2`) programın çalışmasına devam edilir.

## 5. Nesne ve Pointer İzleme (Heap Tracking)
Xbox Kernel'i, oyunların ne kadar bellek talep ettiğinden bağımsız bazı durumlarda tahsis boyutlarını bilmek ister. Ayrıca oyunlar buffer-overflow veya pointer-underflow yapmaya müsaittir.
- **Çözüm (ex.cpp / mm.cpp)**: `ExAllocatePool` çağrılarına tahsis boyutu kadar değil, sonuna +`4096 byte` buffer alanı (padding) eklenerek malloc yapılır. Ayrıca dönen pointer ve asıl boyutu bir `std::unordered_map` içinde tutularak, `MmQueryAllocationSize` sorulduğunda orijinal (veya glibc `malloc_usable_size` ile sarmalanmış) güvenli değer dönülür.

*(Bu doküman XXCL çekirdek kodlamasında sistemin "nasıl" çalıştığını ve kod eklentilerinin "nereye" dahil edileceğini gösteren canlı bir manifestodur.)*
