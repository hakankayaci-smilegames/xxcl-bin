# XXCL (Xbox Compatibility Layer)

XXCL, orijinal Xbox yürütülebilir dosyalarını (XBE) doğrudan Linux üzerinde çalıştırmayı hedefleyen modüler, LLE (Low Level Emulation) ve HLE (High Level Emulation) karması bir uyumluluk katmanıdır. Geleneksel emülatörlerin aksine, oyunları tamamen izole edilmiş bir ortamda sanallaştırmak yerine, Xbox Kernel çağrılarını ve MMIO (Memory Mapped I/O) donanım adreslerini Linux üzerinde POSIX/glibc sistem çağrılarına eşleyerek, işlemcinin komutları native olarak (veya olabildiğince yakın) işlemesini amaçlar.

## Güncel Durum

XBE yükleme, kernel uyumluluğu ve APU/DSP firmware başlatma üzerinde çalışılıyor. Temiz geçici HDD kökündeki ilk açılış reboot istedi. NV2A yazı belleği beklemesi için eklenen model ikinci açılışta ilerleme sağladı. Oyun şimdi `0x001A3238` civarında USER DMA GET ile PUT eşit olana kadar bekliyor. PUT adresi bellek tahsisine göre değişiyor; son izde USER GET `0` kaldı. Komut kuyruğu henüz çalıştırılmıyor. Gerçek çizim başlamadı.

SDL penceresindeki tanılama paneli host döngüsünün FPS ve frametime değerlerini, DSP worker durumunu/PC'sini/işlenen instruction sayısını ve PFIFO DMA GET/PUT/STATUS değerlerini gösterir. Host FPS, oyunun render FPS'i değildir. Worker, NV2A'nın `0xFD100410` yazı belleği aktarım kaydını “bekleyen aktarım yok” durumunda tutar; burada gerçek bir yazı belleği önbelleği modellenmiyor. Etkin DMA kanalında worker USER DMA PUT/GET ile PFIFO Cache1 DMA PUT/GET (`0xFD800040/44`, `0xFD003240/244`) değerlerini eşliyor; canlı izde PUT iki görünümde de aynı, GET ise henüz `0`. Komutlar henüz işlenmiyor. `GPU PENDING` sayacı komutların işlendiğini göstermez. Oyun grafikleri henüz çizilmiyor.

NV2A için test edilebilir bir PFIFO kayıt durumu eklendi ve worker’a bağlandı. Etkin DMA kanalında USER DMA PUT PFIFO’ya aktarılıyor; PFIFO GET ilerlemesi USER GET görünümüne yansıtılıyor. Bu eşleme oyun beklemesini tek başına çözmüyor: GET’i ilerletecek pushbuffer komutları henüz yürütülmüyor.

Pushbuffer için komut başlığı/veri uzunluğu ve RAMIN’deki DMA nesnesinin üç kelimelik alanı sınır kontrolüyle okunabiliyor. Kesik veya geçersiz veri için tüketim miktarı sıfırdır. Bu çözücüler henüz NV2A worker’a bağlanmadı; çalışan oyunun kuyruğu bu nedenle hâlâ işlenmiyor.

Worker, seçili DMA instance numarasını RAMIN’deki nesneye çevirip ham `flags/limit/frame` alanlarını terminale raporluyor. Canlı Sega GT izinde instance offset’i `0x11C0`; bu yalnız teşhis verisidir, koda oyun adresi olarak sabitlenmemiştir.

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
- **NV2A tanılama worker'ı (`nv2a.cpp`)**:
  - Worker, gerçek PFIFO DMA GET/PUT ve STATUS kayıtlarını izler. Daha önceki `0xFD100410` busy-bit temizleme ve `0xFD800040/44` GET/PUT okumaları yanlış adreslere gidiyordu; bunlar kaldırıldı. FIFO komut yürütmesi henüz uygulanmadı.
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

1. **NV2A komut kuyruğu ve ilk çizim**:
   - `NV_USER_DMA_GET/PUT` kayıtlarını PFIFO DMA kayıtlarına bağla, DMA nesnesi ile komut belleğini takip et ve komutları gerçekten tüket. GET değerini yalnız beklemeyi atlatmak için PUT'a eşitleme.
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
