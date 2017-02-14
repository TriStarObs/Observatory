Code for control of the shutter.

This is the secondary (Shutter) of a primary-secondary relationship between the dome 
(rotating the dome itself, serving status info to ASCOM, and
responding to ASCOM commands) and the shutter (responsible only for commanding
the shutter and reporting shutter status to the primary).

Motor 			:	Monster Guts 2-Speed 12VDC Wiper Motor 
					(Any wiper motor should do)
					http://www.monsterguts.com/store/product.php?productid=17685&cat=3&page=1

Controller 		: 	Pololu High-Power Motor Controller 18v15 
					https://www.pololu.com/product/1376

Microprocessor 	:	Arduino UNO v3