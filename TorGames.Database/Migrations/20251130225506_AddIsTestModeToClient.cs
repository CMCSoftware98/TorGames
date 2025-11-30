using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace TorGames.Database.Migrations
{
    /// <inheritdoc />
    public partial class AddIsTestModeToClient : Migration
    {
        /// <inheritdoc />
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.AddColumn<bool>(
                name: "IsTestMode",
                table: "Clients",
                type: "INTEGER",
                nullable: false,
                defaultValue: false);
        }

        /// <inheritdoc />
        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropColumn(
                name: "IsTestMode",
                table: "Clients");
        }
    }
}
