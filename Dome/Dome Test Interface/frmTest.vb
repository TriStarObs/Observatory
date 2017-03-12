Public Class frmTest

    Private driver As ASCOM.DriverAccess.Dome

    ''' <summary>
    ''' This event is where the driver is choosen. The device ID will be saved in the settings.
    ''' </summary>
    ''' <param name="sender">The source of the event.</param>
    ''' <param name="e">The <see cref="System.EventArgs" /> instance containing the event data.</param>
    Private Sub buttonChoose_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles buttonChoose.Click
        My.Settings.DriverId = ASCOM.DriverAccess.Dome.Choose(My.Settings.DriverId)
        SetUIState()
    End Sub

    ''' <summary>
    ''' Connects to the device to be tested.
    ''' </summary>
    ''' <param name="sender">The source of the event.</param>
    ''' <param name="e">The <see cref="System.EventArgs" /> instance containing the event data.</param>
    Private Sub buttonConnect_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles buttonConnect.Click
        If (IsConnected) Then
            driver.Connected = False
            Timer1.Enabled = False
        Else
            driver = New ASCOM.DriverAccess.Dome(My.Settings.DriverId)
            driver.Connected = True
            'Timer1.Enabled = True
            'Timer1.Interval = 1000
        End If
        SetUIState()
    End Sub

    Private Sub Form1_FormClosing(ByVal sender As System.Object, ByVal e As System.Windows.Forms.FormClosingEventArgs) Handles MyBase.FormClosing
        If IsConnected Then
            driver.Connected = False
        End If
        ' the settings are saved automatically when this application is closed.
    End Sub

    ''' <summary>
    ''' Sets the state of the UI depending on the device state
    ''' </summary>
    Private Sub SetUIState()
        buttonConnect.Enabled = Not String.IsNullOrEmpty(My.Settings.DriverId)
        buttonChoose.Enabled = Not IsConnected
        buttonConnect.Text = IIf(IsConnected, "Disconnect", "Connect")
    End Sub

    ''' <summary>
    ''' Gets a value indicating whether this instance is connected.
    ''' </summary>
    ''' <value>
    ''' 
    ''' <c>true</c> if this instance is connected; otherwise, <c>false</c>.
    ''' 
    ''' </value>
    Private ReadOnly Property IsConnected() As Boolean
        Get
            If Me.driver Is Nothing Then Return False
            Return driver.Connected
        End Get
    End Property

    ' TODO: Add additional UI and controls to test more of the driver being tested.

    Private Sub btnOpen_Click(sender As Object, e As EventArgs) Handles btnOpenShutter.Click
        If Me.IsConnected Then
            driver.OpenShutter()
        Else
            MsgBox("Driver is not connected")
        End If
    End Sub

    Private Sub btnCloseShutter_Click(sender As Object, e As EventArgs) Handles btnCloseShutter.Click
        If Me.IsConnected Then
            driver.CloseShutter()
        Else
            MsgBox("Driver is not connected")
        End If

    End Sub

    Private Sub btnStopShutter_Click(sender As Object, e As EventArgs) Handles btnStopShutter.Click
        If Me.IsConnected Then
            driver.Action("StopShutter", "")
        Else
            MsgBox("Driver is not connected")
        End If
    End Sub

    Private Sub btnResetShutter_Click(sender As Object, e As EventArgs) Handles btnResetShutter.Click
        If Me.IsConnected Then
            driver.Action("ResetShutter", "")
        Else
            MsgBox("Driver is not connected")
        End If
    End Sub

    Private Sub btnStatus_Click(sender As Object, e As EventArgs) Handles btnStatus.Click
        txtShutter.Text = driver.ShutterStatus.ToString
    End Sub


    Private Sub Timer1_Tick(sender As Object, e As EventArgs) Handles Timer1.Tick
        txtShutter.Text = driver.ShutterStatus.ToString
        '        txtAzimuth.Text = driver.Azimuth.ToString
    End Sub

    Private Sub btnAzimuth_Click(sender As Object, e As EventArgs) Handles btnAzimuth.Click
        txtAzimuth.Text = driver.Azimuth
    End Sub
End Class
