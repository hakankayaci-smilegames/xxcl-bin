# NV2A ile İlk Görünür Görüntü Tasarımı

## Amaç

XXCL'nin boş tanılama penceresinden çıkıp Xbox oyununun gerçekten gönderdiği bir GPU komutunu işleyerek ekranda görünür bir görüntü üretmesi. İlk hedef tam 3D sahne değil; oyunun pushbuffer'ında bulunan gerçek bir temizleme/renk doldurma komutunun doğru sırayla işlenmesi ve sonucunun SDL penceresinde görünmesidir.

## Kanıt ve başlangıç noktası

- `0xFD100410` NV2A yazı belleği aktarım kaydındaki bekleme, worker bu önbelleği modellemediği için tamamlanmış olarak yanıtlanıyor. İzole ikinci açılışta oyun bu beklemeyi geçti.
- Sonraki guest beklemesi `0x001A3238` civarında `NV_USER_DMA_GET` ile `NV_USER_DMA_PUT` eşit olana kadar sürüyor. Gözlenen değerler USER PUT=`0x04AD8000`, USER GET=`0`; `NV_PFIFO_CACHE1_DMA_PUT/GET` ise ikisi de `0`.
- NV2A referans davranışında USER GET/PUT, seçili kanalın ortak PFIFO DMA kayıtlarına karşılık gelir. Mevcut XXCL eşlemesi bu kayıtları ayrı düz bellek hücreleri olarak tutuyor.
- Oyunun yazdığı `0x04AD8000` yalnızca gözlenmiş bir değerdir. DMA nesnesi ve komutların bellekteki yeri henüz doğrulanmamıştır; bu değer kodda sabitlenmeyecek.

## Tasarım

NV2A kayıtlarının tek bir sahibi olacak. USER DMA GET/PUT adresleri, aktif kanal ve DMA/PIO modu dikkate alınarak ortak PFIFO DMA durumuna yönlendirilecek. Böylece oyun aynı kuyruğun iki adresinden de tutarlı değer okur; kod herhangi bir oyunun gördüğümüz sayılarını özel olarak tanımaz.

NV2A worker, PFIFO DMA kayıtları ve RAMIN/DMA nesnesi üzerinden komutların gerçek kaynak belleğini çözecek. Adres çözümleme mevcut `physical_memory_to_host` servisiyle yapılacak; aralık dışında kalan, hizasız veya okunamayan komut belleği sessizce atlanmayacak. Hata durumunda kuyruğu tüketilmiş gibi göstermeden tanılama bilgisi bırakılacak.

Pushbuffer komutları önce küçük, sözleşme testleri olan bir çözücüden geçirilecek. GET yalnız çözülen komut gerçekten işlendikten sonra ilerleyecek. İlk desteklenen komut kümesi, yeni izde görülen gerçek akışa göre seçilecek; yöntem/adres veya title ID'ye özel kestirme eklenmeyecek.

GPU işlemesinin sonucu, worker'ın SDL çağırması yerine güvenli bir kare/veri çıktısı olarak video katmanına aktarılacak. SDL ana döngüsü bu çıktıyı tanılama panelinin arkasında gösterecek. Böylece ilk görünür temizleme rengi, oyun komutunun gerçekten yürütüldüğünü gösterir; yalnız GET/PUT eşitliği başarı sayılmaz.

## İlk görünür milestone kabul ölçütleri

1. İzole HDD kökünde ilk açılışın reboot isteği ve sonraki açılış aynı koşuda izlenebilir.
2. USER GET/PUT okumaları ortak PFIFO DMA durumuyla tutarlı olur; oyun `0x001A3238` bekleme noktasını geçer.
3. Worker DMA nesnesini ve pushbuffer belleğini gözlenen geçerli adreslerden çözer; en az bir gerçek pushbuffer komutunu tüketir. GET ilerlemesi, işlenmiş komutlarla açıklanabilir.
4. İzde bulunan gerçek temizleme komutu SDL penceresinde görünür bir renkli alan üretir. Overlay görünür kalır.
5. Sözleşme testleri USER/PFIFO alias davranışını, komut çözümünü, adres sınırlarını ve GET'in yalnız işlenen komut sonrasında ilerlemesini kapsar.
6. Depodaki `hdd/` ağacı kullanılmaz veya değiştirilmez. Sega GT yalnız Xbox davranışını sınayan entegrasyon örneğidir; oyun adına bağlı davranış yoktur.

## Kapsam dışı

- Tam D3D/NV2A uyumluluğu, geometri, shader ve doku çizimi.
- Gözlenen PUT değerini GET'e kopyalayarak beklemeyi geçirme.
- DSP, ses, input veya VFS işlerini bu milestone'a ekleme.
- Genel bir donanım zamanlaması ya da cycle-accurate emülasyon iddiası.

## Geliştirme ve geri dönüş noktaları

Her küçük kaynak değişikliği önce hedefli sözleşme kontrolüyle, sonra derlemeyle ve ilgili olduğunda izole runtime koşusuyla doğrulanır. Her doğrulanmış kaynak adımı yalnız kendi ilgili dosyalarıyla commit edilip `main` dalına push edilir. İzlenen derleme çıktısı ve eski debug dosyaları commit kapsamına alınmaz.
