<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class frmTest
    Inherits System.Windows.Forms.Form

    'Form overrides dispose to clean up the component list.
    <System.Diagnostics.DebuggerNonUserCode()> _
    Protected Overrides Sub Dispose(ByVal disposing As Boolean)
        Try
            If disposing AndAlso components IsNot Nothing Then
                components.Dispose()
            End If
        Finally
            MyBase.Dispose(disposing)
        End Try
    End Sub

    'Required by the Windows Form Designer
    Private components As System.ComponentModel.IContainer

    'NOTE: The following procedure is required by the Windows Form Designer
    'It can be modified using the Windows Form Designer.  
    'Do not modify it using the code editor.
    <System.Diagnostics.DebuggerStepThrough()> _
    Private Sub InitializeComponent()
        Me.components = New System.ComponentModel.Container()
        Me.labelDriverId = New System.Windows.Forms.Label()
        Me.buttonConnect = New System.Windows.Forms.Button()
        Me.buttonChoose = New System.Windows.Forms.Button()
        Me.btnOpenShutter = New System.Windows.Forms.Button()
        Me.btnCloseShutter = New System.Windows.Forms.Button()
        Me.btnResetShutter = New System.Windows.Forms.Button()
        Me.btnStopShutter = New System.Windows.Forms.Button()
        Me.txtStatus = New System.Windows.Forms.TextBox()
        Me.btnStatus = New System.Windows.Forms.Button()
        Me.Timer1 = New System.Windows.Forms.Timer(Me.components)
        Me.SuspendLayout()
        '
        'labelDriverId
        '
        Me.labelDriverId.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle
        Me.labelDriverId.DataBindings.Add(New System.Windows.Forms.Binding("Text", Global.ASCOM.TriStarObs.My.MySettings.Default, "DriverId", True, System.Windows.Forms.DataSourceUpdateMode.OnPropertyChanged))
        Me.labelDriverId.Location = New System.Drawing.Point(22, 68)
        Me.labelDriverId.Margin = New System.Windows.Forms.Padding(6, 0, 6, 0)
        Me.labelDriverId.Name = "labelDriverId"
        Me.labelDriverId.Size = New System.Drawing.Size(532, 37)
        Me.labelDriverId.TabIndex = 5
        Me.labelDriverId.Text = Global.ASCOM.TriStarObs.My.MySettings.Default.DriverId
        Me.labelDriverId.TextAlign = System.Drawing.ContentAlignment.MiddleLeft
        '
        'buttonConnect
        '
        Me.buttonConnect.Location = New System.Drawing.Point(579, 66)
        Me.buttonConnect.Margin = New System.Windows.Forms.Padding(6)
        Me.buttonConnect.Name = "buttonConnect"
        Me.buttonConnect.Size = New System.Drawing.Size(132, 42)
        Me.buttonConnect.TabIndex = 4
        Me.buttonConnect.Text = "Connect"
        Me.buttonConnect.UseVisualStyleBackColor = True
        '
        'buttonChoose
        '
        Me.buttonChoose.Location = New System.Drawing.Point(579, 13)
        Me.buttonChoose.Margin = New System.Windows.Forms.Padding(6)
        Me.buttonChoose.Name = "buttonChoose"
        Me.buttonChoose.Size = New System.Drawing.Size(132, 42)
        Me.buttonChoose.TabIndex = 3
        Me.buttonChoose.Text = "Choose"
        Me.buttonChoose.UseVisualStyleBackColor = True
        '
        'btnOpenShutter
        '
        Me.btnOpenShutter.Location = New System.Drawing.Point(22, 159)
        Me.btnOpenShutter.Name = "btnOpenShutter"
        Me.btnOpenShutter.Size = New System.Drawing.Size(239, 50)
        Me.btnOpenShutter.TabIndex = 6
        Me.btnOpenShutter.Text = "Open Shutter"
        Me.btnOpenShutter.UseVisualStyleBackColor = True
        '
        'btnCloseShutter
        '
        Me.btnCloseShutter.Location = New System.Drawing.Point(22, 215)
        Me.btnCloseShutter.Name = "btnCloseShutter"
        Me.btnCloseShutter.Size = New System.Drawing.Size(239, 50)
        Me.btnCloseShutter.TabIndex = 7
        Me.btnCloseShutter.Text = "Close Shutter"
        Me.btnCloseShutter.UseVisualStyleBackColor = True
        '
        'btnResetShutter
        '
        Me.btnResetShutter.Location = New System.Drawing.Point(22, 327)
        Me.btnResetShutter.Name = "btnResetShutter"
        Me.btnResetShutter.Size = New System.Drawing.Size(239, 50)
        Me.btnResetShutter.TabIndex = 8
        Me.btnResetShutter.Text = "Reset Shutter"
        Me.btnResetShutter.UseVisualStyleBackColor = True
        '
        'btnStopShutter
        '
        Me.btnStopShutter.Location = New System.Drawing.Point(22, 271)
        Me.btnStopShutter.Name = "btnStopShutter"
        Me.btnStopShutter.Size = New System.Drawing.Size(239, 50)
        Me.btnStopShutter.TabIndex = 9
        Me.btnStopShutter.Text = "Stop Shutter"
        Me.btnStopShutter.UseVisualStyleBackColor = True
        '
        'txtStatus
        '
        Me.txtStatus.Location = New System.Drawing.Point(370, 195)
        Me.txtStatus.Name = "txtStatus"
        Me.txtStatus.Size = New System.Drawing.Size(315, 29)
        Me.txtStatus.TabIndex = 10
        '
        'btnStatus
        '
        Me.btnStatus.Location = New System.Drawing.Point(22, 383)
        Me.btnStatus.Name = "btnStatus"
        Me.btnStatus.Size = New System.Drawing.Size(239, 50)
        Me.btnStatus.TabIndex = 11
        Me.btnStatus.Text = "Get Status"
        Me.btnStatus.UseVisualStyleBackColor = True
        '
        'frmTest
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(11.0!, 24.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.ClientSize = New System.Drawing.Size(733, 517)
        Me.Controls.Add(Me.btnStatus)
        Me.Controls.Add(Me.txtStatus)
        Me.Controls.Add(Me.btnStopShutter)
        Me.Controls.Add(Me.btnResetShutter)
        Me.Controls.Add(Me.btnCloseShutter)
        Me.Controls.Add(Me.btnOpenShutter)
        Me.Controls.Add(Me.labelDriverId)
        Me.Controls.Add(Me.buttonConnect)
        Me.Controls.Add(Me.buttonChoose)
        Me.Margin = New System.Windows.Forms.Padding(6)
        Me.Name = "frmTest"
        Me.Text = "Dome Driver Test Client"
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub
    Private WithEvents labelDriverId As System.Windows.Forms.Label
    Private WithEvents buttonConnect As System.Windows.Forms.Button
    Private WithEvents buttonChoose As System.Windows.Forms.Button
    Friend WithEvents btnOpenShutter As System.Windows.Forms.Button
    Friend WithEvents btnCloseShutter As System.Windows.Forms.Button
    Friend WithEvents btnResetShutter As System.Windows.Forms.Button
    Friend WithEvents btnStopShutter As System.Windows.Forms.Button
    Friend WithEvents txtStatus As System.Windows.Forms.TextBox
    Friend WithEvents btnStatus As System.Windows.Forms.Button
    Friend WithEvents Timer1 As System.Windows.Forms.Timer

End Class
