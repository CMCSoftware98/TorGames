using System;
using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace TorGames.Database.Migrations
{
    /// <inheritdoc />
    public partial class InitialCreate : Migration
    {
        /// <inheritdoc />
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.CreateTable(
                name: "Clients",
                columns: table => new
                {
                    ClientId = table.Column<string>(type: "TEXT", maxLength: 64, nullable: false),
                    ClientType = table.Column<string>(type: "TEXT", maxLength: 32, nullable: false),
                    MachineName = table.Column<string>(type: "TEXT", maxLength: 256, nullable: false),
                    Username = table.Column<string>(type: "TEXT", maxLength: 256, nullable: false),
                    OsVersion = table.Column<string>(type: "TEXT", maxLength: 256, nullable: false),
                    OsArchitecture = table.Column<string>(type: "TEXT", maxLength: 32, nullable: false),
                    CpuCount = table.Column<int>(type: "INTEGER", nullable: false),
                    TotalMemoryBytes = table.Column<long>(type: "INTEGER", nullable: false),
                    MacAddress = table.Column<string>(type: "TEXT", maxLength: 64, nullable: false),
                    LastIpAddress = table.Column<string>(type: "TEXT", maxLength: 45, nullable: false),
                    CountryCode = table.Column<string>(type: "TEXT", maxLength: 8, nullable: false),
                    IsAdmin = table.Column<bool>(type: "INTEGER", nullable: false),
                    IsUacEnabled = table.Column<bool>(type: "INTEGER", nullable: false),
                    ClientVersion = table.Column<string>(type: "TEXT", maxLength: 32, nullable: false),
                    Label = table.Column<string>(type: "TEXT", maxLength: 512, nullable: true),
                    FirstSeenAt = table.Column<DateTime>(type: "TEXT", nullable: false),
                    LastSeenAt = table.Column<DateTime>(type: "TEXT", nullable: false),
                    TotalConnections = table.Column<int>(type: "INTEGER", nullable: false),
                    IsFlagged = table.Column<bool>(type: "INTEGER", nullable: false),
                    IsBlocked = table.Column<bool>(type: "INTEGER", nullable: false),
                    IsTestMode = table.Column<bool>(type: "INTEGER", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_Clients", x => x.ClientId);
                });

            migrationBuilder.CreateTable(
                name: "ClientConnections",
                columns: table => new
                {
                    Id = table.Column<int>(type: "INTEGER", nullable: false)
                        .Annotation("Sqlite:Autoincrement", true),
                    ClientId = table.Column<string>(type: "TEXT", maxLength: 64, nullable: false),
                    IpAddress = table.Column<string>(type: "TEXT", maxLength: 45, nullable: false),
                    CountryCode = table.Column<string>(type: "TEXT", maxLength: 8, nullable: false),
                    ClientVersion = table.Column<string>(type: "TEXT", maxLength: 32, nullable: false),
                    ConnectedAt = table.Column<DateTime>(type: "TEXT", nullable: false),
                    DisconnectedAt = table.Column<DateTime>(type: "TEXT", nullable: true),
                    DurationSeconds = table.Column<long>(type: "INTEGER", nullable: true),
                    DisconnectReason = table.Column<string>(type: "TEXT", maxLength: 64, nullable: true)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_ClientConnections", x => x.Id);
                    table.ForeignKey(
                        name: "FK_ClientConnections_Clients_ClientId",
                        column: x => x.ClientId,
                        principalTable: "Clients",
                        principalColumn: "ClientId",
                        onDelete: ReferentialAction.Cascade);
                });

            migrationBuilder.CreateTable(
                name: "CommandLogs",
                columns: table => new
                {
                    Id = table.Column<int>(type: "INTEGER", nullable: false)
                        .Annotation("Sqlite:Autoincrement", true),
                    CommandId = table.Column<string>(type: "TEXT", maxLength: 64, nullable: false),
                    ClientId = table.Column<string>(type: "TEXT", maxLength: 64, nullable: true),
                    CommandType = table.Column<string>(type: "TEXT", maxLength: 64, nullable: false),
                    CommandText = table.Column<string>(type: "TEXT", nullable: false),
                    SentAt = table.Column<DateTime>(type: "TEXT", nullable: false),
                    WasDelivered = table.Column<bool>(type: "INTEGER", nullable: false),
                    ResultReceivedAt = table.Column<DateTime>(type: "TEXT", nullable: true),
                    Success = table.Column<bool>(type: "INTEGER", nullable: true),
                    ResultOutput = table.Column<string>(type: "TEXT", nullable: true),
                    ErrorMessage = table.Column<string>(type: "TEXT", maxLength: 1024, nullable: true),
                    IsBroadcast = table.Column<bool>(type: "INTEGER", nullable: false),
                    InitiatorIp = table.Column<string>(type: "TEXT", maxLength: 45, nullable: true)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_CommandLogs", x => x.Id);
                    table.ForeignKey(
                        name: "FK_CommandLogs_Clients_ClientId",
                        column: x => x.ClientId,
                        principalTable: "Clients",
                        principalColumn: "ClientId",
                        onDelete: ReferentialAction.SetNull);
                });

            migrationBuilder.CreateIndex(
                name: "IX_ClientConnections_ClientId",
                table: "ClientConnections",
                column: "ClientId");

            migrationBuilder.CreateIndex(
                name: "IX_ClientConnections_ConnectedAt",
                table: "ClientConnections",
                column: "ConnectedAt");

            migrationBuilder.CreateIndex(
                name: "IX_ClientConnections_IpAddress",
                table: "ClientConnections",
                column: "IpAddress");

            migrationBuilder.CreateIndex(
                name: "IX_Clients_CountryCode",
                table: "Clients",
                column: "CountryCode");

            migrationBuilder.CreateIndex(
                name: "IX_Clients_IsBlocked",
                table: "Clients",
                column: "IsBlocked");

            migrationBuilder.CreateIndex(
                name: "IX_Clients_IsFlagged",
                table: "Clients",
                column: "IsFlagged");

            migrationBuilder.CreateIndex(
                name: "IX_Clients_LastSeenAt",
                table: "Clients",
                column: "LastSeenAt");

            migrationBuilder.CreateIndex(
                name: "IX_Clients_MachineName",
                table: "Clients",
                column: "MachineName");

            migrationBuilder.CreateIndex(
                name: "IX_Clients_Username",
                table: "Clients",
                column: "Username");

            migrationBuilder.CreateIndex(
                name: "IX_CommandLogs_ClientId",
                table: "CommandLogs",
                column: "ClientId");

            migrationBuilder.CreateIndex(
                name: "IX_CommandLogs_CommandId",
                table: "CommandLogs",
                column: "CommandId");

            migrationBuilder.CreateIndex(
                name: "IX_CommandLogs_CommandType",
                table: "CommandLogs",
                column: "CommandType");

            migrationBuilder.CreateIndex(
                name: "IX_CommandLogs_SentAt",
                table: "CommandLogs",
                column: "SentAt");
        }

        /// <inheritdoc />
        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropTable(
                name: "ClientConnections");

            migrationBuilder.DropTable(
                name: "CommandLogs");

            migrationBuilder.DropTable(
                name: "Clients");
        }
    }
}
