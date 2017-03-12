'tabs=4
' --------------------------------------------------------------------------------
'
' ASCOM Dome driver for TristarObs
'
' Description:	ASCOM Dome driver for TriStar Observatory
'
' Implements:	ASCOM Dome interface version: 1.0
' Author:		(Eor) EorEquis@tristarobservatory.space
'
' Edit Log:
'
' Date			Who	Vers	Description
' -----------	---	-----	-------------------------------------------------------
' 18-FEB-2017	Eor	0.1.0	Initial edit, from Dome template
' ---------------------------------------------------------------------------------
'
'
' Driver ID : ASCOM.TristarObs.Dome
'
#Const Device = "Dome"

Imports ASCOM
Imports ASCOM.Astrometry
Imports ASCOM.Astrometry.AstroUtils
Imports ASCOM.DeviceInterface
Imports ASCOM.Utilities

Imports System
Imports System.Collections
Imports System.Collections.Generic
Imports System.Globalization
Imports System.Runtime.InteropServices
Imports System.Text

<Guid("fad9f7cd-358c-4519-9e2f-ec9275a2750d")> _
<ClassInterface(ClassInterfaceType.None)> _
Public Class Dome
    ' The Guid attribute sets the CLSID for ASCOM.TristarObs.Dome
    ' The ClassInterface/None addribute prevents an empty interface called
    ' _TristarObs from being created and used as the [default] interface
    Implements IDomeV2

    '
    ' Driver ID and descriptive string that shows in the Chooser
    '
    Friend Shared driverID As String = "ASCOM.TristarObs.Dome"
    Private Shared driverDescription As String = "TristarObs Dome"

    Friend Shared comPortProfileName As String = "COM Port" 'Constants used for Profile persistence
    Friend Shared traceStateProfileName As String = "Trace Level"
    Friend Shared comPortDefault As String = "COM7"
    Friend Shared traceStateDefault As String = "False"

    Friend Shared comPort As String ' Variables to hold the currrent device configuration
    Friend Shadows portNum As String
    Friend Shared traceState As Boolean

    Private connectedState As Boolean ' Private variable to hold the connected state
    Private utilities As Util ' Private variable to hold an ASCOM Utilities object
    Private astroUtilities As AstroUtils ' Private variable to hold an AstroUtils object to provide the Range method
    Private TL As TraceLogger ' Private variable to hold the trace logger object (creates a diagnostic log file with information that you specify)
    Private objSerial As ASCOM.Utilities.Serial

    Dim WithEvents Timer1 As New System.Timers.Timer

    '
    ' Constructor - Must be public for COM registration!
    '
    Public Sub New()

        ReadProfile() ' Read device configuration from the ASCOM Profile store
        TL = New TraceLogger("", "TristarObs")
        TL.Enabled = traceState
        TL.LogMessage("Dome", "Starting initialisation")

        connectedState = False ' Initialise connected to false
        utilities = New Util() ' Initialise util object
        astroUtilities = New AstroUtils 'Initialise new astro utiliites object

        TL.LogMessage("Dome", "Completed initialisation")
    End Sub

    '
    ' PUBLIC COM INTERFACE IDomeV2 IMPLEMENTATION
    '

#Region "Common properties and methods"
    Public Sub SetupDialog() Implements IDomeV2.SetupDialog
        If IsConnected Then
            ' TODO : Show "command center" dialog here
            System.Windows.Forms.MessageBox.Show("Already connected, just press OK")
        End If

        Using F As SetupDialogForm = New SetupDialogForm()
            Dim result As System.Windows.Forms.DialogResult = F.ShowDialog()
            If result = DialogResult.OK Then
                WriteProfile() ' Persist device configuration values to the ASCOM Profile store
            End If
        End Using
    End Sub

    Public ReadOnly Property SupportedActions() As ArrayList Implements IDomeV2.SupportedActions
        Get
            TL.LogMessage("SupportedActions Get", "Returning ActionsList (StopShutter, ResetShutter)")
            Dim ActionsList As New ArrayList
            ActionsList.Add("Dome:StopShutter")
            ActionsList.Add("Dome:ResetShutter")
            ActionsList.Add("Dome:GetStatus")
            Return ActionsList
        End Get
    End Property

    Public Function Action(ByVal ActionName As String, ByVal ActionParameters As String) As String Implements IDomeV2.Action
        Select Case ActionName
            Case "StopShutter"
                Try
                    CommandBlind("xxxx#")
                    Return "Shutter stopped"
                Catch ex As Exception
                    Throw New ApplicationException("Unable to stop shutter", ex)
                End Try
            Case "ResetShutter"
                Try
                    CommandBlind("rset#")
                    Return "Shutter Reset"
                Catch ex As Exception
                    Throw New ApplicationException("Unable to reset shutter", ex)
                End Try
            Case "GetStatus"
                Try
                    Dim strStatus As String
                    strStatus = CommandString("snfo#")
                    Return strStatus
                Catch ex As Exception
                    Throw New ApplicationException("Unable to get status", ex)
                End Try

            Case Else
                Throw New ActionNotImplementedException("Action " & ActionName & " is not supported by this driver")
        End Select
    End Function

    Public Sub CommandBlind(ByVal Command As String, Optional ByVal Raw As Boolean = False) Implements IDomeV2.CommandBlind
        Try
            CheckConnected("CommandBlind")
            objSerial.Transmit(Command)
        Catch ex As Exception
            Throw New ApplicationException("CommandBlind failed.", ex)
        End Try
    End Sub

    Public Function CommandBool(ByVal Command As String, Optional ByVal Raw As Boolean = False) As Boolean _
        Implements IDomeV2.CommandBool
        CheckConnected("CommandBool")
        Dim ret As String = CommandString(Command, Raw)
        ' TODO decode the return string and return true or false
        ' or
        Throw New MethodNotImplementedException("CommandBool")
    End Function

    Public Function CommandString(ByVal Command As String, Optional ByVal Raw As Boolean = False) As String _
        Implements IDomeV2.CommandString
        ' TODO : something to ensure that only one command is in progress at a time
        Try
            Dim response As String
            CheckConnected("CommandString")
            objSerial.Transmit(Command)
            response = objSerial.ReceiveTerminated("#")
            response = response.Replace("#", "")
            Return response
        Catch ex As Exception
            Return "255"
            ' Throw New ApplicationException("CommandString failed.", ex)
        End Try
    End Function

    Public Property Connected() As Boolean Implements IDomeV2.Connected
        Get
            TL.LogMessage("Connected Get", IsConnected.ToString())
            Return IsConnected
        End Get
        Set(value As Boolean)
            TL.LogMessage("Connected Set", value.ToString())
            If value = IsConnected Then
                Return
            End If

            If value Then
                connectedState = True
                TL.LogMessage("Connected Set", "Connecting to port " + comPort)
                portNum = Right(comPort, Len(comPort) - 3)
                objSerial = New ASCOM.Utilities.Serial
                objSerial.Port = CInt(portNum)
                objSerial.Speed = SerialSpeed.ps115200
                objSerial.Connected = True
            Else
                connectedState = False
                TL.LogMessage("Connected Set", "Disconnecting from port " + comPort)

            End If
        End Set
    End Property

    Public ReadOnly Property Description As String Implements IDomeV2.Description
        Get
            ' this pattern seems to be needed to allow a public property to return a private field
            Dim d As String = driverDescription
            TL.LogMessage("Description Get", d)
            Return d
        End Get
    End Property

    Public ReadOnly Property DriverInfo As String Implements IDomeV2.DriverInfo
        Get
            Dim m_version As Version = System.Reflection.Assembly.GetExecutingAssembly().GetName().Version
            Dim s_driverInfo As String = "TriStar Observatory Dome Driver. Version: " + m_version.Major.ToString() + "." + m_version.Minor.ToString()
            TL.LogMessage("DriverInfo Get", s_driverInfo)
            Return s_driverInfo
        End Get
    End Property

    Public ReadOnly Property DriverVersion() As String Implements IDomeV2.DriverVersion
        Get
            ' Get our own assembly and report its version number
            TL.LogMessage("DriverVersion Get", Reflection.Assembly.GetExecutingAssembly.GetName.Version.ToString(2))
            Return Reflection.Assembly.GetExecutingAssembly.GetName.Version.ToString(2)
        End Get
    End Property

    Public ReadOnly Property InterfaceVersion() As Short Implements IDomeV2.InterfaceVersion
        Get
            TL.LogMessage("InterfaceVersion Get", "2")
            Return 2
        End Get
    End Property

    Public ReadOnly Property Name As String Implements IDomeV2.Name
        Get
            Dim s_name As String = "TriStarObs Dome"
            TL.LogMessage("Name Get", s_name)
            Return s_name
        End Get
    End Property

    Public Sub Dispose() Implements IDomeV2.Dispose
        ' Clean up the tracelogger and util objects
        TL.Enabled = False
        TL.Dispose()
        TL = Nothing
        utilities.Dispose()
        utilities = Nothing
        astroUtilities.Dispose()
        astroUtilities = Nothing
    End Sub

#End Region

#Region "IDome Implementation"

    ' Private domeShutterState As Boolean = False ' Variable to hold the open/closed status of the shutter, true = Open
    Private dblAzimuth As Double = 0

    Public Sub AbortSlew() Implements IDomeV2.AbortSlew
        ' TODO : Slew abort code for dome.  Also see if this stops the shutter
        TL.LogMessage("AbortSlew", "Completed")
    End Sub

    Public ReadOnly Property Altitude() As Double Implements IDomeV2.Altitude
        Get
            ' Dome does not support shutter altitude.
            TL.LogMessage("Altitude Get", "Not implemented")
            Throw New ASCOM.PropertyNotImplementedException("Altitude", False)
        End Get
    End Property

    Public ReadOnly Property AtHome() As Boolean Implements IDomeV2.AtHome
        Get
            ' TODO : Implement
            TL.LogMessage("AtHome", "Not implemented")
            Throw New ASCOM.PropertyNotImplementedException("AtHome", False)
        End Get
    End Property

    Public ReadOnly Property AtPark() As Boolean Implements IDomeV2.AtPark
        Get
            ' TODO : Implement
            TL.LogMessage("AtPark", "Not implemented")
            Throw New ASCOM.PropertyNotImplementedException("AtPark", False)
        End Get
    End Property

    Public ReadOnly Property Azimuth() As Double Implements IDomeV2.Azimuth
        Get
            dblAzimuth = CDbl(CommandString("dazm#", True))
            TL.LogMessage("Azimuth", dblAzimuth.ToString)
            Return dblAzimuth
        End Get
    End Property

    Public ReadOnly Property CanFindHome() As Boolean Implements IDomeV2.CanFindHome
        Get
            ' TODO : Implement
            TL.LogMessage("CanFindHome Get", False.ToString())
            Return False
        End Get
    End Property

    Public ReadOnly Property CanPark() As Boolean Implements IDomeV2.CanPark
        Get
            ' TODO : Implement
            TL.LogMessage("CanPark Get", False.ToString())
            Return False
        End Get
    End Property

    Public ReadOnly Property CanSetAltitude() As Boolean Implements IDomeV2.CanSetAltitude
        Get
            ' Dome cannot set altitude
            TL.LogMessage("CanSetAltitude Get", False.ToString())
            Return False
        End Get
    End Property

    Public ReadOnly Property CanSetAzimuth() As Boolean Implements IDomeV2.CanSetAzimuth
        Get
            ' TODO : Implement
            TL.LogMessage("CanSetAzimuth Get", False.ToString())
            Return False
        End Get
    End Property

    Public ReadOnly Property CanSetPark() As Boolean Implements IDomeV2.CanSetPark
        Get
            ' TODO : Implement
            TL.LogMessage("CanSetPark Get", False.ToString())
            Return False
        End Get
    End Property

    Public ReadOnly Property CanSetShutter() As Boolean Implements IDomeV2.CanSetShutter
        Get
            TL.LogMessage("CanSetShutter Get", True.ToString())
            Return True
        End Get
    End Property

    Public ReadOnly Property CanSlave() As Boolean Implements IDomeV2.CanSlave
        Get
            ' Return false.  Many clients now handle slaving themselves.  Dome cannot handle its own slaving
            TL.LogMessage("CanSlave Get", False.ToString())
            Return False
        End Get
    End Property

    Public ReadOnly Property CanSyncAzimuth() As Boolean Implements IDomeV2.CanSyncAzimuth
        Get
            ' TODO : Implement
            TL.LogMessage("CanSyncAzimuth Get", False.ToString())
            Return False
        End Get
    End Property

    Public Sub CloseShutter() Implements IDomeV2.CloseShutter
        CommandBlind("shcl#", True)
        TL.LogMessage("CloseShutter", "Shutter has been closed")
        'domeShutterState = False
    End Sub

    Public Sub FindHome() Implements IDomeV2.FindHome
        ' TODO : Implement
        TL.LogMessage("FindHome", "Not implemented")
        Throw New ASCOM.MethodNotImplementedException("FindHome")
    End Sub

    Public Sub OpenShutter() Implements IDomeV2.OpenShutter
        TL.LogMessage("OpenShutter", "Shutter has been opened")
        CommandBlind("shop#", True)
    End Sub

    Public Sub Park() Implements IDomeV2.Park
        ' TODO : Implement
        TL.LogMessage("Park", "Not implemented")
        Throw New ASCOM.MethodNotImplementedException("Park")
    End Sub

    Public Sub SetPark() Implements IDomeV2.SetPark
        ' TODO : Implement
        TL.LogMessage("SetPark", "Not implemented")
        Throw New ASCOM.MethodNotImplementedException("SetPark")
    End Sub

    Public ReadOnly Property ShutterStatus() As ShutterState Implements IDomeV2.ShutterStatus
        Get

            Dim intShutterStatus As Integer = CInt(CommandString("snfo#"))
            If intShutterStatus = 255 Then
                Return ShutterState.shutterError
            Else
                Dim strBinary As String = Convert.ToString(intShutterStatus, 2).PadLeft(8, "0"c)
                strBinary = StrReverse(strBinary)
                If GetBit(strBinary, 4) Or GetBit(strBinary, 5) Then
                    TL.LogMessage("shutterstatus", ShutterState.shutterError.ToString())
                    Return ShutterState.shutterError
                ElseIf GetBit(strBinary, 0) Then
                    TL.LogMessage("shutterstatus", ShutterState.shutterOpen.ToString())
                    Return ShutterState.shutterOpen
                ElseIf GetBit(strBinary, 1) Then
                    TL.LogMessage("shutterstatus", ShutterState.shutterClosed.ToString())
                    Return ShutterState.shutterClosed
                ElseIf GetBit(strBinary, 2) Then
                    TL.LogMessage("shutterstatus", ShutterState.shutterOpening.ToString())
                    Return ShutterState.shutterOpening
                ElseIf GetBit(strBinary, 3) Then
                    TL.LogMessage("shutterstatus", ShutterState.shutterClosing.ToString())
                    Return ShutterState.shutterClosing
                End If
            End If
        End Get
    End Property

    Public Property Slaved() As Boolean Implements IDomeV2.Slaved
        ' Return false.  Many clients now handle slaving themselves.  Dome cannot handle its own slaving.  Throw exception if set.
        Get
            TL.LogMessage("Slaved Get", False.ToString())
            Return False
        End Get
        Set(value As Boolean)
            TL.LogMessage("Slaved Set", "not implemented")
            Throw New ASCOM.PropertyNotImplementedException("Slaved", True)
        End Set
    End Property

    Public Sub SlewToAltitude(Altitude As Double) Implements IDomeV2.SlewToAltitude
        TL.LogMessage("SlewToAltitude", "Not implemented")
        Throw New ASCOM.MethodNotImplementedException("SlewToAltitude")
    End Sub

    Public Sub SlewToAzimuth(Azimuth As Double) Implements IDomeV2.SlewToAzimuth
        ' TODO : Implement
        TL.LogMessage("SlewToAzimuth", "Not implemented")
        Throw New ASCOM.MethodNotImplementedException("SlewToAzimuth")
    End Sub

    Public ReadOnly Property Slewing() As Boolean Implements IDomeV2.Slewing
        ' TODO : Implement
        Get
            TL.LogMessage("Slewing Get", False.ToString())
            Return False
        End Get
    End Property

    Public Sub SyncToAzimuth(Azimuth As Double) Implements IDomeV2.SyncToAzimuth
        ' TODO : Implement
        TL.LogMessage("SyncToAzimuth", "Not implemented")
        Throw New ASCOM.MethodNotImplementedException("SyncToAzimuth")
    End Sub

#End Region

#Region "Private properties and methods"


#Region "ASCOM Registration"

    Private Shared Sub RegUnregASCOM(ByVal bRegister As Boolean)

        Using P As New Profile() With {.DeviceType = "Dome"}
            If bRegister Then
                P.Register(driverID, driverDescription)
            Else
                P.Unregister(driverID)
            End If
        End Using

    End Sub

    <ComRegisterFunction()> _
    Public Shared Sub RegisterASCOM(ByVal T As Type)

        RegUnregASCOM(True)

    End Sub

    <ComUnregisterFunction()> _
    Public Shared Sub UnregisterASCOM(ByVal T As Type)

        RegUnregASCOM(False)

    End Sub

#End Region

    Private ReadOnly Property IsConnected As Boolean
        Get
            ' TODO check that the driver hardware connection exists and is connected to the hardware
            Return connectedState
        End Get
    End Property

    Private Sub CheckConnected(ByVal message As String)
        If Not IsConnected Then
            Throw New NotConnectedException(message)
        End If
    End Sub

    Friend Sub ReadProfile()
        Using driverProfile As New Profile()
            driverProfile.DeviceType = "Dome"
            traceState = Convert.ToBoolean(driverProfile.GetValue(driverID, traceStateProfileName, String.Empty, traceStateDefault))
            comPort = driverProfile.GetValue(driverID, comPortProfileName, String.Empty, comPortDefault)
        End Using
    End Sub

    Friend Sub WriteProfile()
        Using driverProfile As New Profile()
            driverProfile.DeviceType = "Dome"
            driverProfile.WriteValue(driverID, traceStateProfileName, traceState.ToString())
            driverProfile.WriteValue(driverID, comPortProfileName, comPort.ToString())
        End Using

    End Sub

    Private Function GetBit(ByVal BinaryString As String, ByVal position As Integer) As Boolean
        Return (Mid(BinaryString, position + 1, 1) = "1")
    End Function

#End Region

End Class
