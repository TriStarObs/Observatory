Code for control of the observatory

This code is split into a primary-secondary relationship between the dome 
(rotating the dome itself, serving status info to ASCOM, and
responding to ASCOM commands) and the shutter (responsible only for commanding
the shutter and reporting shutter status to the primary).

The layers will be :

Arduino - Microprocessor code for commanding hardware

ASCOM - An ASCOM (http://www.ascom-standards.org/) compliant driver to provide
		access to the non-standardized hardware code.