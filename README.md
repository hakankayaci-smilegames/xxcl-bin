# XXCL (Xbox Compatibility Layer)

XXCL, orijinal Xbox yürütülebilir dosyalarını (XBE) doğrudan Linux üzerinde çalıştırmayı hedefleyen modüler, LLE (Low Level Emulation) ve HLE (High Level Emulation) karması bir uyumluluk katmanıdır. Geleneksel emülatörlerin aksine, oyunları tamamen izole edilmiş bir ortamda sanallaştırmak yerine, Xbox Kernel çağrılarını ve MMIO (Memory Mapped I/O) donanım adreslerini Linux üzerinde POSIX/glibc sistem çağrılarına eşleyerek, işlemcinin komutları native olarak (veya olabildiğince yakın) işlemesini amaçlar.

## Güncel Durum

XBE yükleme, kernel uyumluluğu, APU/DSP firmware başlatma ve gözlenen GP DSP DMA aktarımı üzerinde çalışılıyor. Son Sega GT entegrasyon koşusu DSP firmware'ini `PC=0x171` noktasına kadar ilerletti; burada henüz desteklenmeyen `P=0x000000` komutunda duruyor. Bu checkpoint oyun özelinde bir geçiş değildir; sıradaki DSP davranışını belirlemek için kullanılan entegrasyon kanıtıdır.

SDL penceresindeki tanılama paneli host döngüsünün FPS ve frametime değerlerini, DSP worker durumunu/PC'sini/işlenen instruction sayısını ve NV2A FIFO GET/PUT değerlerini gösterir. Host FPS, oyunun render FPS'i değildir. NV2A worker şimdilik FIFO GET işaretçisini PUT'a eşitler; pushbuffer komutları ayrıştırılmadığı için oyun grafikleri henüz çizilmiyor.

## 🏗️ Mimari ve Şematik (Architecture)

Aşağıdaki şema, XXCL'nin donanım kaynaklarını ve kernel çağrılarını (HLE) nasıl Linux POSIX fonksiyonlarına bağladığını özetlemektedir:

```mermaid
flowchart TD
    subgraph XBE_Application [Xbox XBE (Native Game Code)]
        Code[Game Execution]
        API_Call[Kernel API Calls (__stdcall)]
        Mem_Access[Direct MMIO Access]
        IO_Access[IN/OUT Port Access]
    end

    subgraph XXCL_Emulator [XXCL Compatibility Layer]
        Loader[XBE Loader & Section Mapper]
        
        subgraph Kernel_Subsystem [Xbox Kernel Emulation]
            K_Ke[Ke - Kernel Exec]
            K_Mm[Mm - Memory Manager]
            K_Hal[Hal - Hardware Abstraction]
            K_Io[Io - I/O Manager]
            K_Ps[Ps - Process/Thread]
            K_Ex[Ex - Executive]
            K_DS[DSound - DirectSound Stub]
        end
        
        subgraph Hardware_Simulation [Hardware & MMIO Simulation]
            NV2A[NV2A Worker Thread]
            APU[APU/DSP MMIO Tracker]
            APIC[APIC / IRQ Controller]
            SigSegv[SIGSEGV Fault Handler]
        end
    end

    subgraph Linux_OS [Linux OS (Host)]
        Glibc[glibc / pthread / malloc]
        MMap[mmap / Memory Protection]
        VFS[Linux Filesystem]
        Signals[Signal Dispatcher (SIGSEGV)]
    end

    %% Flow connections
    Code --> API_Call
    API_Call --> Kernel_Subsystem
    Code --> Mem_Access
    Mem_Access -- "Page Fault" --> SigSegv
    Code --> IO_Access
    IO_Access -- "Privilege Fault" --> SigSegv

    SigSegv --> APU
    SigSegv --> APIC
    SigSegv --> NV2A

    K_Mm --> MMap
    K_Ex --> Glibc
    K_Ps --> Glibc
    K_Io --> VFS
    
    Loader --> MMap
```

### Temel Bileşenler
1. **XBE Loader**: XBE dosyasının header'ını okuyarak kodu belleğe (0x10000 tabanına) yerleştirir ve Import Table (Thunk) çözünürlüğünü sağlayıp Kernel çağrılarını XXCL fonksiyonlarına bağlar.
2. **Memory Manager (Mm) & Executive (Ex)**: Xbox belleğinin kısıtlı yapısını (64MB sınırını ve bellek bitişikliğini) POSIX `mmap` ile taklit eder. Allocation limitleri ve padding izlenerek XBE içindeki hatalı hafıza okumalarının Host glibc'yi çökertmesi önlenir.
3. **Hardware & MMIO**: Xbox oyunlarının doğrudan okuyup yazdığı fiziksel adreslere (örneğin APU, APIC, NV2A FIFO) denk gelen sanal adres bölgeleri `mmap` ile `PROT_NONE` yapılarak, veya özel worker thread'ler ile (puller emülasyonu) canlı tutularak oyunun donma (spin-loop) ve çökme yaşaması önlenir.
4. **Signal Handler (SIGSEGV)**: Oyun I/O portlarına (`out`, `in`) veya korunmuş hafızalara (WBINVD gibi kernel komutları) ulaştığında Linux `SIGSEGV` yollar. XXCL bu sinyalleri yakalayarak komutu atlar veya simüle eder.

---

## ✅ Tamamlananlar (Done)

- **APU ve APIC MMIO Eşlemeleri (`hal.cpp`)**: 
  - `0xFE800000` (APU) ve `0xFEC00000` (APIC) aralıklarındaki register erişimlerinden kaynaklı çökmeler çözüldü. Bu alanlar allocate edilerek null-pointer/segfault okumaları güvenli hale getirildi.
- **Linux Heap Çökmelerinin Giderilmesi (Memory Tracking)**: 
  - `ExAllocatePool` üzerinden alınan bellekleri oyun sınır dışına (out-of-bounds) taşırarak Linux glibc heap yapısını bozuyordu (malloc/free assertion failure). Tüm tahsislere `4096 bayt` padding eklendi ve gerçek boyutları `unordered_map` ile takip edilerek (`MmQueryAllocationSize` vb.) glibc'nin çökmesi %100 engellendi.
- **NV2A FIFO Asenkron Worker (`nv2a.cpp`)**: 
  - Sega GT 2002 GPU/NV2A donanımına (PFIFO CACHE1 STATUS - `0xFD100410`) yazıp FIFO'nun boşalmasını beklerken (spin-loop) %100 CPU harcayarak kilitleniyordu. Bir arka plan `pthread` yaratıldı, bu thread düzenli olarak GPU register'ını "işi bitmiş/boş" (busy=0) şeklinde güncelleyerek oyun döngüsünün kırılıp devam etmesi sağlandı.
- **Host tanılama overlay'i (`video/window.cpp`)**:
  - Host FPS/frametime ile APU DSP ve NV2A FIFO sayaçlarını gösterir. Bu değerler emülatör döngüsünü izler; Xbox oyununun gerçek çizim hızı olarak yorumlanmamalıdır.
- **Fiziksel Adres Maskelemesi**:
  - `MmGetPhysicalAddress`, sanal adresleri doğrudan değil, `0x03FFFFFF` (64MB) maskesinden geçirerek Xbox fiziksel bellek uzayına (%100 uygun) döndürmeye başladı.
- **EEPROM Parametrelerinin Mocklanması (`ex.cpp`)**:
  - `ExQueryNonVolatileSetting` (Ordinal 24) sıfır döndüğü için DSound başlatılamıyordu. Fonksiyon düzenlenerek *Language*, *Video*, *Audio*, *Game Region*, *AV Region* gibi Xbox EEPROM anahtarlarına doğru `DWORD` değerleri (örneğin Audio=0, Region=4 (Europe)) dönmesi sağlandı.
- **I/O Port Simülasyonu (SIGSEGV)**:
  - Sega GT'nin SMBus kontrolcüsüne (`out %al, (%dx)` vb.) doğrudan I/O erişim yapmasından kaynaklı SIGSEGV'ler `sigsegv_handler` içinde yakalanarak `EIP` ilerletmesiyle atlatıldı.

---

## 🚧 Yapılacaklar (To-Do / Roadmap)

1. **GP DSP firmware yürütmesi**:
   - Güncel checkpoint `PC=0x171`, `P=0x000000`. Önce komutun DSP56300 anlamı belirlenip oyun bağımsız sözleşme testiyle uygulanmalı; ses komut tamamlama davranışı gerçek APU/DSP olaylarına bağlanmalı.
2. **D3D ve NV2A gerçek çizimi**:
   - FIFO GET/PUT eşitlemesi pushbuffer yürütmesi değildir. İlk çizim hedefi, guest'in NV2A pushbuffer'ından genel bir clear komutunu okuyup host penceresine yansıtmaktır; ardından temel state, vertex ve texture yolları eklenmelidir.
3. **Gamepad / Input entegrasyonu**:
   - `XInputGetState` gibi input API çağrıları Host işletim sistemindeki evdev/SDL katmanına bağlanacak.
4. **Dosya sistemi (VFS) zenginleştirmesi**:
   - Oyun DVD'den (veya T:\\, Z:\\ gibi sanal Xbox bölümlerinden) veri çekmeye çalıştığında Host dosya sistemi hiyerarşisine doğru yönlendirilmesi için sembolik link (VFS) map algoritması genişletilecek.

---

## 💻 Sistem Gereksinimleri ve Platform
- **İşletim Sistemi:** Linux (Özellikle CachyOS/Kubuntu/Arch türevleri)
- **Mimari:** x86 (Orijinal Xbox oyunları 32-bit olduğundan 32-bit derleme destekleri ve kütüphaneleri şarttır - glibc.i686 vb.)
- **Derleme:** GCC / CMake (C++20 standartları önerilir)

## ⚖️ Kurallar ve İlkeler
1. **Genel Uyumluluk (Sıfır Oyun Özel Hack):** Yapılan hiçbir yama veya kod parçası sadece *Sega GT 2002* için özel tasarlanamaz. Fonksiyonlar tüm Xbox oyunlarının genel uyumluluğunu sağlayacak şekilde Kernel standartlarına göre yazılmalıdır.
2. **Hands Off Git:** Bu proje geliştirilirken yapay zeka ajanları tarafından Git sıfırlama, dal değiştirme komutları (`git checkout`, `git reset` vb.) kesinlikle çalıştırılmaz. Versiyon kontrolü tamamen kullanıcıya aittir.
3. **Çift Dilli Destek:** Proje en başından itibaren Türkçe ve İngilizce dil mantığına uygun bir temel ile geliştirilmektedir.
