# Workspace Instructions & Behavioral Rules

## 1. Deep Analysis & Planning First (Think Before Coding)
- **Önce Plan ve Analiz:** Her mekanik, sistem veya özellik eklemesinden önce derinlemesine mimari analiz ve adım adım plan çıkar.
- **Kullanıcı Onayı Şart:** Kullanıcı onayı almadan doğrudan büyük kod değişikliklerine başlama.
- **Varsayımları Açıkça Belirt:** Asla varsayımda bulunma, kafa karışıklığını gizleme. Belirsizlik varsa dur, neyin kafa karıştırdığını açıkça belirt ve sor.
- **Alternatifleri ve Tradeoff'ları Sun:** Birden fazla yaklaşım/yorum varsa sessizce kendi kafana göre seçme; seçenekleri ve artı-eksilerini sun.
- **Daha Basit Yol Varsa Söyle:** Eğer daha basit ve temiz bir yöntem varsa çekinmeden söyle ("push back yap").

## 2. Simplicity First (Minimum Kod, Sıfır Spekülasyon)
- **Gereksiz Ekleme Yok:** Sorunu çözen minimum kod; istenmeyen hiçbir ekstra özellik, esneklik veya konfigürasyon ekleme.
- **Aşırı Soyutlamadan Kaçın:** Tek kullanımlık durumlar için gereksiz soyutlama katmanları (abstractions) oluşturma.
- **Defensive Kod Fazlalığı Yok:** İmkansız senaryolar için gereksiz hata yakalamalar veya karmaşık kontrol blokları yazma.
- **Sadeleştir:** 50 satırda çözülebilecek bir iş için 200 satır yazma; kıdemli mühendis sadeliğiyle temiz kod yaz.

## 3. Surgical Changes (Cerrahi Değişiklikler)
- **Sadece Gereken Yere Dokun:** Yalnızca dokunman gereken satırlara dokun. Kendi yarattığın izleri temizle, başkasınınkini elleme.
- **Çalışan Koda Dokunma:** İlgisiz çevre kodları, yorumları veya formatları "düzeltmeye/iyileştirmeye" kalkışma. Bozuk olmayan hiçbir şeyi refactor etme.
- **Mevcut Stile Birebir Uy:** Projenin var olan kod stiline, değişken isimlendirmelerine ve mimarisine %100 uyum sağla.
- **Öksüz Kod Temizliği:** Senin yaptığın değişiklikler sonucu boşa çıkan (orphan) import, değişken veya fonksiyonları temizle; fakat önceden var olan ölü kodları istenmedikçe silme.
- **Kriter:** Değişen her satır doğrudan kullanıcının isteğiyle doğrudan bağlantılı olmalıdır.

## 4. Goal-Driven Execution & Test Doğrulama Protokolü
- **Doğrulanabilir Hedefler:** Görevleri somut ve doğrulanabilir hedeflere dönüştür.
- **Adım Adım Doğrulama:** Çok adımlı görevlerde kısa ve net plan yap: `1. [Adım] → kontrol: [check]`.
- **Statik Tip Güvenliği:** Kod yazarken statik tip güvenliğine (`var speed: float = 100.0`), Godot 4.x standartlarına ve talimatlara %100 uy.
- **Gereksiz Test Harness Yasak:** Kendi kendine gereksiz/sentetik headless test scriptleri (`SceneTree` harness vb.) çalıştırarak vakit ve context harcama.
- **Kullanıcıdan Test İste:** Kodundan ve mimarisinden emin olduğun an işi tamamla ve doğrudan **Kullanıcıdan Test Feedback'i İste**.

## 5. Kod ve Mimari Standartları
- Godot 4.x 2D, statik tipli GDScript(kullanıldığında).
- Veri odaklı, modüler ve kolay düzenlenebilir anlaşılabilir yapı.
- CachyOS Linux ortamı, son derece doğrudan ve filtresiz iletişim tarzı ("kanka"), sıfır laf kalabalığı (zero fluff).

## 6. Proje Dokümantasyonu & Mimari Senkronizasyonu (Architecture Sync)
- Yeni projeye başlandığında veya mimari yapının kavranması gerektiğinde, tüm kod ve yapılandırma dosyalarını okuyarak bağlantıları açıklayan bir `PROJECT_ARCHITECTURE.md` oluşturmayı öner veya oluştur.
- Projede kod, sahne, resource veya mekanik değişikliği yapıldığında; `PROJECT_ARCHITECTURE.md` dokümanını zorunlu olarak senkronize ve güncel tut.

## 7. Git ve Versiyon Kontrol Dokunulmazlığı (Hands Off Git)
- **Git ile Dosya Değiştirme / Sıfırlama Kesinlikle Yasak:** Kullanıcı açıkça istemediği sürece (`git checkout`, `git restore`, `git reset`, `git revert`, `git clean` vb.) hiçbir dosya geri alma/sıfırlama komutu çalıştırma.
- **Kullanıcının Değişikliklerine Saygı:** Çalışma alanındaki (`git status`) herhangi bir değişiklik kullanıcının kendi geliştirmesi olabilir. Asla kendi inisiyatifinle "temizlik" ya da revert yapma.
- **Yalnızca Salt Okunur:** `git diff` veya `git status` dışında git state'ini değiştiren hiçbir eyleme girişme; commit, stash, checkout ve dal yönetimini tamamen kullanıcıya bırak.

## 8. Optimizasyon & Mantık Bütünlüğü (Logic Preservation)
- **Çalışma Mantığını Asla Bozma:** Optimizasyon ve performans iyileştirmelerinde (donma, lag spike, fps düşüşü çözümlerinde) esas hedef donmayı çözerken sistemin var olan çalışma mantığını, oyun kurallarını, veri akışını ve hesaplamalarını asla bozmamaktır.

## 9. Çift Dilli Yerelleştirme İlkesi (Bilingual Localization First)
- **Programlar yapıldığı ilk andan itibaren en az Türkçe/İngilizce dil desteğine sahip olmalı, ve dil ekleme yapısı oldukça kolay ve modüler olmalıdır**

