# PrismCNC
Free 3 axis CNC project

![Picture of the hardware](hardware.jpg)


Software:
The backend is written in C++11 and the original runs on a [pcDuino-V3-nano](http://pcduino.eu/pcduino-3-nano/).
You might want to replace it with a [Pine64](https://www.pine64.com) or [Raspberry PI](https://www.raspberrypi.org),
but keep in mind that these need more extras like a SD card.
You can use [my Blender addon](https://github.com/Lichtso/curve_cad) to design and export toolpaths in G-Code.


Example hardware implementation:

Euro    | US Dollar  | Name and Link
------- | ---------- | -------------------
255.00€ | $286.54    | [Miller](http://www.sorotec.de/shop/Proxxon/Proxxon-Micromot/MICRO-miller-MF-70.html)
135.00€ | $151.70    | [CNC-Kit Proxxon MF 70](http://www.ebay.com/itm/221921493227)
49.99€  | $56.17     | [Mainboard](http://www.conrad.de/ce/de/product/1274214/pcDuino-V3-nano-Einplatinencomputer-Linux-Ubuntu-Version-1204-Arduino-I-D-E)
30.62€  | $34.41     | [Motor Drivers](http://www.ebay.com/itm/141918401264)
23.05€  | $25.90     | [Power Supply](http://www.reichelt.de/Schaltnetzteile-Case-geschlossen/SNT-RD-65A/3/index.html?&ACTION=3&LA=2&ARTICLE=137100&GROUPID=4959&artnr=SNT+RD+65A)
17.80€  | $20.00     | [Casing](http://www.reichelt.de/Teko-Alu-Gehaeuse/TEKO-383/3/index.html?&ACTION=3&LA=2&ARTICLE=21196&GROUPID=5201&artnr=TEKO+383)
7.20€   | $8.09      | [Connectors](http://www.reichelt.de/G-Serie/G4-W1F/3/index.html?&ACTION=3&LA=2&ARTICLE=52077&GROUPID=3263&artnr=G4+W1F)
5.10€   | $5.73      | [Connectors](http://www.reichelt.de/G-Serie/G4-A5M/3/index.html?&ACTION=3&LA=2&ARTICLE=52079&GROUPID=3263&artnr=G4+A5M)
7.30€   | $8.20      | [Connectors](http://www.reichelt.de/Kaltgeraeteeinbaustecker/KM-01-1105/3/index.html?&ACTION=3&LA=2&ARTICLE=44537&GROUPID=5204&artnr=KM+01.1105)
1.30€   | $1.46       | [Fuse](http://www.reichelt.de/Kaltgeraeteeinbaustecker/KM-01SH-1/3/index.html?&ACTION=3&LA=2&ARTICLE=58884&GROUPID=5204&artnr=KM+01SH-1)
0.77€   | $0.87       | [Fuse](http://www.reichelt.de/5x20mm-Feinsicherungen/FLINK-4-0A/3/index.html?&ACTION=3&LA=2&ARTICLE=7837&GROUPID=3301&artnr=FLINK+4%2C0A)
0.81€   | $0.91       | [Button](http://www.reichelt.de/Drucktaster-Druckschalter/S-1323-RT/3/index.html?&ACTION=3&LA=2&ARTICLE=15414&GROUPID=3277&artnr=S+1323+RT)
2.95€   | $3.31       | [Shrink Tubing](http://www.pollin.de/shop/dt/NDI3OTkxOTk-/Bauelemente_Bauteile/Sortimente/Sonstiges/Schrumpfschlauch_Sortiment_schwarz.html)
0.50€   | $0.56       | [Pins](http://www.pollin.de/shop/dt/NzIzOTU1OTk-/Bauelemente_Bauteile/Mechanische_Bauelemente/Steckverbinder_Klemmen/Stiftleiste.html)
2.95€   | $3.31       | [Wires](http://www.pollin.de/shop/dt/Njk5OTkxOTk-/Haustechnik/Kabel_Draehte_Litzen/Litzen/Litzen_Sortiment_0_14_mm_5x_5_m.html)
1.80€   | $2.02       | [Wires](http://www.pollin.de/shop/dt/MzI1ODQ1OTk-/Bauelemente_Bauteile/Mechanische_Bauelemente/Steckverbinder_Klemmen/Buchsenleiste_Serie_PS.html)
0.90€   | $1.01       | [Wires](http://www.pollin.de/shop/dt/ODI1ODQ1OTk-/Bauelemente_Bauteile/Mechanische_Bauelemente/Steckverbinder_Klemmen/Buchsenleiste_Serie_PS.html)
0.90€   | $1.01       | [Connectors](http://www.pollin.de/shop/dt/MzQxOTQ1OTk-/Bauelemente_Bauteile/Mechanische_Bauelemente/Steckverbinder_Klemmen/Leiterplatten_Anschlussklemme_XY301V.html)
4.20€   | $4.72       | [Connectors](http://www.pollin.de/shop/dt/NTIzODQ1OTk-/Bauelemente_Bauteile/Mechanische_Bauelemente/Steckverbinder_Klemmen/Anschlussklemme_PTR_AKZ950_2_polig_gruen.html)
1.56€   | $1.75       | [Connectors](http://www.pollin.de/shop/dt/MDIzODQ1OTk-/Bauelemente_Bauteile/Mechanische_Bauelemente/Steckverbinder_Klemmen/Stiftleiste_PTR_STLZ950_2_polig_90_gruen.html)
549.70€ | $617.70     | Converted (€ -> $) sum
600.00€ | $674.22     | Realistic actual sum

Screws and delivery costs are not included and like any other DIY project too, you are welcome to add you own parts, ideas and variations.
Last checked 2016/07/26
